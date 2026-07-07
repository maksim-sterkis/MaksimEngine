#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "deprecated/stb_image_resize.h"

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <ktx.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <cstring>
#include <cmath>

namespace fs = std::filesystem;

std::vector<std::byte> read_file_bytes(const std::string& inputPath) {
    std::ifstream file(inputPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<std::byte> result(size);
    if (file.read(reinterpret_cast<char*>(result.data()), size)) {
        return result;
    }
    return {};
}

void compile_gltf_model(const std::string& inputPath, const std::string& outputPath) {
    fastgltf::Parser parser;
    auto data = fastgltf::GltfDataBuffer::FromPath(inputPath);
    if (data.error() != fastgltf::Error::None) {
        std::cerr << "Failed to load GLTF data: " << inputPath << "\n";
        return;
    }

    auto assetResult = parser.loadGltf(data.get(), std::filesystem::path(inputPath).parent_path(), fastgltf::Options::LoadExternalBuffers);
    if (assetResult.error() != fastgltf::Error::None) {
        std::cerr << "Failed to parse GLTF: " << inputPath << "\n";
        return;
    }

    auto& asset = assetResult.get();
    
    bool hasBasisu = false;
    for (const auto& ext : asset.extensionsUsed) {
        if (ext == "KHR_texture_basisu") hasBasisu = true;
    }
    if (!hasBasisu) {
        asset.extensionsUsed.push_back("KHR_texture_basisu");
        asset.extensionsRequired.push_back("KHR_texture_basisu");
    }

    std::filesystem::path baseDir = std::filesystem::path(inputPath).parent_path();

    // Merge all existing buffers into one to comply with GLB requirement of a single binary chunk
    std::vector<std::byte> mergedBuffer;
    std::vector<size_t> bufferOffsets(asset.buffers.size());

    for (size_t i = 0; i < asset.buffers.size(); ++i) {
        while (mergedBuffer.size() % 4 != 0) mergedBuffer.push_back(std::byte{0});
        bufferOffsets[i] = mergedBuffer.size();
        
        if (auto* arr = std::get_if<fastgltf::sources::Array>(&asset.buffers[i].data)) {
            mergedBuffer.insert(mergedBuffer.end(), arr->bytes.begin(), arr->bytes.end());
        } else if (auto* vec = std::get_if<fastgltf::sources::Vector>(&asset.buffers[i].data)) {
            mergedBuffer.insert(mergedBuffer.end(), vec->bytes.begin(), vec->bytes.end());
        }
    }

    for (auto& bv : asset.bufferViews) {
        bv.byteOffset += bufferOffsets[bv.bufferIndex];
        bv.bufferIndex = 0;
    }

    for (size_t i = 0; i < asset.images.size(); ++i) {
        auto& image = asset.images[i];
        std::string imgPath;
        if (auto* uri = std::get_if<fastgltf::sources::URI>(&image.data)) {
            imgPath = (baseDir / uri->uri.path()).string();
        } else {
            continue;
        }

        std::cout << "  Embedding texture: " << imgPath << "\n";
        
        auto rawData = read_file_bytes(imgPath);
        if (!rawData.empty()) {
            while (mergedBuffer.size() % 4 != 0) mergedBuffer.push_back(std::byte{0});
            size_t offset = mergedBuffer.size();
            
            mergedBuffer.insert(mergedBuffer.end(), rawData.begin(), rawData.end());

            fastgltf::BufferView bView;
            bView.bufferIndex = 0;
            bView.byteOffset = offset;
            bView.byteLength = rawData.size();
            
            size_t bViewIndex = asset.bufferViews.size();
            asset.bufferViews.push_back(std::move(bView));

            fastgltf::sources::BufferView bViewSource;
            bViewSource.bufferViewIndex = bViewIndex;
            
            std::string ext = fs::path(imgPath).extension().string();
            if (ext == ".jpg" || ext == ".jpeg") bViewSource.mimeType = fastgltf::MimeType::JPEG;
            else if (ext == ".png") bViewSource.mimeType = fastgltf::MimeType::PNG;
            else bViewSource.mimeType = fastgltf::MimeType::None;

            image.data = std::move(bViewSource);
        } else {
            std::cerr << "  Failed to read texture: " << imgPath << "\n";
        }
    }

    fastgltf::Buffer mainBuffer;
    mainBuffer.byteLength = mergedBuffer.size();
    fastgltf::sources::Vector vecSource;
    vecSource.bytes = std::move(mergedBuffer);
    mainBuffer.data = std::move(vecSource);
    
    asset.buffers.clear();
    asset.buffers.push_back(std::move(mainBuffer));

    fastgltf::FileExporter exporter;
    auto error = exporter.writeGltfBinary(asset, outputPath);
    if (error != fastgltf::Error::None) {
        std::cerr << "Failed to export GLB: " << fastgltf::getErrorMessage(error) << "\n";
    } else {
        std::cout << "Successfully exported GLB to " << outputPath << "\n";
    }
}


struct Vertex {
    float pos[3];
    float color[3];
    float normal[3];
    float texCoord[2];
};

void compile_model(const std::string& inputPath, const std::string& outputPath) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, inputPath.c_str())) {
        std::cerr << "Failed to load OBJ: " << warn << err << "\n";
        return;
    }

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    float faceColors[6][3] = {
        {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f}
    };
    int vertexIndex = 0;

    for (const auto& shape : shapes) {
        for (size_t i = 0; i < shape.mesh.indices.size(); i += 3) {
            Vertex v[3];
            for (int j = 0; j < 3; j++) {
                auto index = shape.mesh.indices[i + j];
                v[j].pos[0] = attrib.vertices[3 * index.vertex_index + 0];
                v[j].pos[1] = attrib.vertices[3 * index.vertex_index + 1];
                v[j].pos[2] = attrib.vertices[3 * index.vertex_index + 2];

                if (index.normal_index >= 0) {
                    v[j].normal[0] = attrib.normals[3 * index.normal_index + 0];
                    v[j].normal[1] = attrib.normals[3 * index.normal_index + 1];
                    v[j].normal[2] = attrib.normals[3 * index.normal_index + 2];
                } else {
                    v[j].normal[0] = 0; v[j].normal[1] = 0; v[j].normal[2] = 0;
                }

                if (index.texcoord_index >= 0) {
                    v[j].texCoord[0] = attrib.texcoords[2 * index.texcoord_index + 0];
                    v[j].texCoord[1] = 1.0f - attrib.texcoords[2 * index.texcoord_index + 1];
                } else {
                    v[j].texCoord[0] = 0; v[j].texCoord[1] = 0;
                }

                if (attrib.colors.size() > 3 * index.vertex_index + 2) {
                    v[j].color[0] = attrib.colors[3 * index.vertex_index + 0];
                    v[j].color[1] = attrib.colors[3 * index.vertex_index + 1];
                    v[j].color[2] = attrib.colors[3 * index.vertex_index + 2];
                } else {
                    v[j].color[0] = faceColors[(vertexIndex / 3) % 6][0];
                    v[j].color[1] = faceColors[(vertexIndex / 3) % 6][1];
                    v[j].color[2] = faceColors[(vertexIndex / 3) % 6][2];
                }
                vertexIndex++;
            }

            if (shape.mesh.indices[i].normal_index < 0) {
                float dx1 = v[1].pos[0] - v[0].pos[0];
                float dy1 = v[1].pos[1] - v[0].pos[1];
                float dz1 = v[1].pos[2] - v[0].pos[2];
                float dx2 = v[2].pos[0] - v[0].pos[0];
                float dy2 = v[2].pos[1] - v[0].pos[1];
                float dz2 = v[2].pos[2] - v[0].pos[2];
                float nx = dy1 * dz2 - dz1 * dy2;
                float ny = dz1 * dx2 - dx1 * dz2;
                float nz = dx1 * dy2 - dy1 * dx2;
                float len = std::sqrt(nx*nx + ny*ny + nz*nz);
                if (len > 0.0f) { nx /= len; ny /= len; nz /= len; }
                for (int j = 0; j < 3; j++) {
                    v[j].normal[0] = nx; v[j].normal[1] = ny; v[j].normal[2] = nz;
                }
            }

            for (int j = 0; j < 3; j++) {
                indices.push_back(static_cast<uint32_t>(vertices.size()));
                vertices.push_back(v[j]);
            }
        }
    }

    std::vector<std::byte> bufferData(vertices.size() * sizeof(Vertex) + indices.size() * sizeof(uint32_t));
    std::memcpy(bufferData.data(), vertices.data(), vertices.size() * sizeof(Vertex));
    std::memcpy(bufferData.data() + vertices.size() * sizeof(Vertex), indices.data(), indices.size() * sizeof(uint32_t));

    fastgltf::Asset asset;
    
    // Buffer
    fastgltf::Buffer buffer;
    buffer.byteLength = bufferData.size();
    fastgltf::sources::Vector vecSource;
    vecSource.bytes = std::move(bufferData);
    buffer.data = std::move(vecSource);
    asset.buffers.push_back(std::move(buffer));

    // BufferViews
    fastgltf::BufferView vView;
    vView.bufferIndex = 0;
    vView.byteOffset = 0;
    vView.byteLength = vertices.size() * sizeof(Vertex);
    vView.byteStride = sizeof(Vertex);
    vView.target = fastgltf::BufferTarget::ArrayBuffer;
    asset.bufferViews.push_back(std::move(vView));

    fastgltf::BufferView iView;
    iView.bufferIndex = 0;
    iView.byteOffset = vertices.size() * sizeof(Vertex);
    iView.byteLength = indices.size() * sizeof(uint32_t);
    iView.target = fastgltf::BufferTarget::ElementArrayBuffer;
    asset.bufferViews.push_back(std::move(iView));

    // Accessors
    fastgltf::Accessor posAcc;
    posAcc.bufferViewIndex = 0;
    posAcc.byteOffset = offsetof(Vertex, pos);
    posAcc.componentType = fastgltf::ComponentType::Float;
    posAcc.type = fastgltf::AccessorType::Vec3;
    posAcc.count = vertices.size();
    asset.accessors.push_back(std::move(posAcc));

    fastgltf::Accessor normAcc;
    normAcc.bufferViewIndex = 0;
    normAcc.byteOffset = offsetof(Vertex, normal);
    normAcc.componentType = fastgltf::ComponentType::Float;
    normAcc.type = fastgltf::AccessorType::Vec3;
    normAcc.count = vertices.size();
    asset.accessors.push_back(std::move(normAcc));

    fastgltf::Accessor texAcc;
    texAcc.bufferViewIndex = 0;
    texAcc.byteOffset = offsetof(Vertex, texCoord);
    texAcc.componentType = fastgltf::ComponentType::Float;
    texAcc.type = fastgltf::AccessorType::Vec2;
    texAcc.count = vertices.size();
    asset.accessors.push_back(std::move(texAcc));

    fastgltf::Accessor colAcc;
    colAcc.bufferViewIndex = 0;
    colAcc.byteOffset = offsetof(Vertex, color);
    colAcc.componentType = fastgltf::ComponentType::Float;
    colAcc.type = fastgltf::AccessorType::Vec3;
    colAcc.count = vertices.size();
    asset.accessors.push_back(std::move(colAcc));

    fastgltf::Accessor indAcc;
    indAcc.bufferViewIndex = 1;
    indAcc.byteOffset = 0;
    indAcc.componentType = fastgltf::ComponentType::UnsignedInt;
    indAcc.type = fastgltf::AccessorType::Scalar;
    indAcc.count = indices.size();
    asset.accessors.push_back(std::move(indAcc));

    // Mesh
    fastgltf::Mesh mesh;
    fastgltf::Primitive prim;
    prim.attributes.emplace_back(fastgltf::Attribute{"POSITION", 0});
    prim.attributes.emplace_back(fastgltf::Attribute{"NORMAL", 1});
    prim.attributes.emplace_back(fastgltf::Attribute{"TEXCOORD_0", 2});
    prim.attributes.emplace_back(fastgltf::Attribute{"COLOR_0", 3});
    prim.indicesAccessor = 4;
    prim.type = fastgltf::PrimitiveType::Triangles;
    mesh.primitives.push_back(std::move(prim));
    asset.meshes.push_back(std::move(mesh));

    // Node
    fastgltf::Node node;
    node.meshIndex = 0;
    asset.nodes.push_back(std::move(node));

    // Scene
    fastgltf::Scene scene;
    scene.nodeIndices.push_back(0);
    asset.scenes.push_back(std::move(scene));
    asset.defaultScene = 0;

    // Export
    fastgltf::FileExporter exporter;
    auto error = exporter.writeGltfBinary(asset, outputPath);
    if (error != fastgltf::Error::None) {
        std::cerr << "Failed to export GLB: " << fastgltf::getErrorMessage(error) << "\n";
    } else {
        std::cout << "Successfully exported " << outputPath << "\n";
    }
}

int main() {
    std::string inDir = "assets/models/obj";
    std::string outDir = "assets/models/glb";

    if (!fs::exists(inDir)) {
        fs::create_directories(inDir);
    }
    if (!fs::exists(outDir)) {
        fs::create_directories(outDir);
    }

    for (const auto& entry : fs::directory_iterator(inDir)) {
        if (!entry.is_regular_file()) continue;

        std::string ext = entry.path().extension().string();
        if (ext == ".obj" || ext == ".gltf") {
            std::string filename = entry.path().stem().string();
            std::string outFile = outDir + "/" + filename + ".glb";

            bool needsUpdate = true;
            if (fs::exists(outFile)) {
                auto inTime = fs::last_write_time(entry.path());
                auto outTime = fs::last_write_time(outFile);
                if (inTime <= outTime) {
                    needsUpdate = false;
                }
            }

            if (needsUpdate) {
                std::cout << "Compiling model: " << entry.path().string() << "...\n";
                if (ext == ".obj") {
                    compile_model(entry.path().string(), outFile);
                } else if (ext == ".gltf") {
                    compile_gltf_model(entry.path().string(), outFile);
                }
            }
        }
    }
    return 0;
}
