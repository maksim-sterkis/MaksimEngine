#include "tiny_obj_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "deprecated/stb_image_resize.h"

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <ktx.h>
#include <meshoptimizer.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <cstring>
#include <cmath>
#include <glm/glm.hpp>

namespace fs = std::filesystem;

void normalize_winding_order(const float* positions, size_t posStride, const float* normals, size_t normStride, uint32_t* indices, size_t indexCount) {
    if (!normals) return;
    int flippedCount = 0;
    for (size_t i = 0; i < indexCount; i += 3) {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i+1];
        uint32_t i2 = indices[i+2];
        
        const float* p0 = (const float*)((const char*)positions + i0 * posStride);
        const float* p1 = (const float*)((const char*)positions + i1 * posStride);
        const float* p2 = (const float*)((const char*)positions + i2 * posStride);
        
        glm::vec3 v0 = glm::vec3(p0[0], p0[1], p0[2]);
        glm::vec3 v1 = glm::vec3(p1[0], p1[1], p1[2]);
        glm::vec3 v2 = glm::vec3(p2[0], p2[1], p2[2]);
        
        const float* n0_ptr = (const float*)((const char*)normals + i0 * normStride);
        glm::vec3 n0 = glm::vec3(n0_ptr[0], n0_ptr[1], n0_ptr[2]);
        
        glm::vec3 geo_normal = glm::cross(v1 - v0, v2 - v0);
        
        if (glm::dot(geo_normal, n0) < 0.0f) {
            indices[i+1] = i2;
            indices[i+2] = i1;
            flippedCount++;
        }
    }
    if (flippedCount > 0) {
        std::cout << "  Fixed winding order: Flipped " << flippedCount << " CW triangles to CCW.\n";
    }
}

// ---- Meshlet file format ----
// Written as a sidecar .meshlet binary alongside the .glb

struct MeshletBoundsGPU {
    float center[3];
    float radius;
    float cone_axis[3];
    float cone_cutoff;
};

struct MeshletFileHeader {
    uint32_t magic;         // 'MESH' = 0x4853454D
    uint32_t version;       // 1
    uint32_t meshletCount;
    uint32_t meshletVertexCount;
    uint32_t meshletTriangleCount;
    uint32_t vertexCount;   // total vertices in the original mesh
    uint32_t indexCount;    // total indices in the original mesh
    uint32_t padding;
};

static const uint32_t MESHLET_MAGIC = 0x4853454D; // "MESH" in little-endian

// Generates meshlets from a flat vertex/index buffer and writes a .meshlet sidecar file.
// vertexPositions: float3 positions (stride = sizeof(float)*3, or sizeof(Vertex) if interleaved)
// vertexStride: byte stride between consecutive vertex positions
void generate_meshlets(const float* vertexPositions, size_t vertexStride,
                       size_t vertexCount,
                       const uint32_t* indices, size_t indexCount,
                       const std::string& outputPath) {
    if (indexCount < 3 || vertexCount < 3) {
        std::cout << "  Skipping meshlet generation (too few vertices/indices)\n";
        return;
    }

    if (std::filesystem::exists(outputPath) && std::filesystem::file_size(outputPath) >= 12) {
        std::ifstream check(outputPath, std::ios::binary | std::ios::ate);
        if (check.is_open()) {
            check.seekg(-4, std::ios::end);
            char magic[5] = {0};
            check.read(magic, 4);
            if (std::string(magic) == "MESH") {
                std::cout << "  GLB already contains meshlets, skipping append.\n";
                return;
            }
        }
    }

    const size_t maxVertices = 64;
    const size_t maxTriangles = 124;
    const float coneWeight = 0.5f;

    std::cout << "  Generating meshlets: " << vertexCount << " verts, " << indexCount << " indices\n";

    size_t maxMeshlets = meshopt_buildMeshletsBound(indexCount, maxVertices, maxTriangles);

    std::vector<meshopt_Meshlet> meshlets(maxMeshlets);
    std::vector<uint32_t> meshletVertices(maxMeshlets * maxVertices);
    std::vector<uint8_t> meshletTriangles(maxMeshlets * maxTriangles * 3);

    size_t meshletCount = meshopt_buildMeshlets(
        meshlets.data(), meshletVertices.data(), meshletTriangles.data(),
        indices, indexCount,
        vertexPositions, vertexCount, vertexStride,
        maxVertices, maxTriangles, coneWeight);

    if (meshletCount == 0) {
        std::cerr << "  meshopt_buildMeshlets returned 0 meshlets, skipping.\n";
        return;
    }

    // Trim to actual size
    const meshopt_Meshlet& last = meshlets[meshletCount - 1];
    size_t totalVertices = last.vertex_offset + last.vertex_count;
    size_t totalTriangles = last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3);
    meshletVertices.resize(totalVertices);
    meshletTriangles.resize(totalTriangles);
    meshlets.resize(meshletCount);

    // Compute bounds for each meshlet (bounding sphere + normal cone for culling)
    std::vector<MeshletBoundsGPU> bounds(meshletCount);
    for (size_t i = 0; i < meshletCount; ++i) {
        meshopt_Bounds b = meshopt_computeMeshletBounds(
            &meshletVertices[meshlets[i].vertex_offset],
            &meshletTriangles[meshlets[i].triangle_offset],
            meshlets[i].triangle_count,
            vertexPositions, vertexCount, vertexStride);

        bounds[i].center[0] = b.center[0];
        bounds[i].center[1] = b.center[1];
        bounds[i].center[2] = b.center[2];
        bounds[i].radius = b.radius;
        bounds[i].cone_axis[0] = b.cone_axis[0];
        bounds[i].cone_axis[1] = b.cone_axis[1];
        bounds[i].cone_axis[2] = b.cone_axis[2];
        bounds[i].cone_cutoff = b.cone_cutoff;
    }

    // Write .meshlet file
    MeshletFileHeader header{};
    header.magic = MESHLET_MAGIC;
    header.version = 1;
    header.meshletCount = static_cast<uint32_t>(meshletCount);
    header.meshletVertexCount = static_cast<uint32_t>(totalVertices);
    header.meshletTriangleCount = static_cast<uint32_t>(totalTriangles);
    header.vertexCount = static_cast<uint32_t>(vertexCount);
    header.indexCount = static_cast<uint32_t>(indexCount);

    // Get current file size
    uint64_t startPos = std::filesystem::file_size(outputPath);

    // Append to GLB file
    std::ofstream out(outputPath, std::ios::binary | std::ios::app);
    out.seekp(0, std::ios::end);

    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    out.write(reinterpret_cast<const char*>(meshlets.data()), meshletCount * sizeof(meshopt_Meshlet));
    out.write(reinterpret_cast<const char*>(meshletVertices.data()), totalVertices * sizeof(uint32_t));
    out.write(reinterpret_cast<const char*>(meshletTriangles.data()), totalTriangles * sizeof(uint8_t));
    out.write(reinterpret_cast<const char*>(bounds.data()), meshletCount * sizeof(MeshletBoundsGPU));
    
    std::streampos endPos = out.tellp();
    uint64_t payloadSize = static_cast<uint64_t>(endPos) - startPos;

    // Write footer (8 bytes: 64-bit size, then 4 byte magic "MESH")
    // Wait, 8 + 4 = 12 bytes. So size is 8 bytes, magic is 4.
    out.write(reinterpret_cast<const char*>(&payloadSize), sizeof(payloadSize));
    out.write("MESH", 4);

    out.close();

    std::cout << "  Appended meshlets to GLB: " << meshletCount << " meshlets, "
              << totalVertices << " vertices, payload size " << payloadSize << " bytes -> " << outputPath << "\n";
}

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
    std::cerr << "[compile_gltf_model] " << inputPath << "\n" << std::flush;
    fastgltf::Parser parser(fastgltf::Extensions::KHR_texture_basisu);
    
    // Read file manually and pad to avoid simdjson AVX page boundary segfaults
    auto fileBytes = read_file_bytes(inputPath);
    if (fileBytes.empty()) {
        std::cerr << "Failed to read GLTF file: " << inputPath << "\n";
        return;
    }
    
    size_t actualSize = fileBytes.size();
    void* alignedPtr = _aligned_malloc(actualSize + 64, 64);
    std::memcpy(alignedPtr, fileBytes.data(), actualSize);
    std::memset(static_cast<std::byte*>(alignedPtr) + actualSize, 0, 64);

    auto dataResult = fastgltf::GltfDataBuffer::FromBytes(static_cast<const std::byte*>(alignedPtr), actualSize);
    if (dataResult.error() != fastgltf::Error::None) {
        std::cerr << "Failed to create GLTF buffer from bytes\n";
        _aligned_free(alignedPtr);
        return;
    }
    auto& data = dataResult.get();

    auto assetResult = parser.loadGltf(data, std::filesystem::path(inputPath).parent_path(), fastgltf::Options::LoadExternalBuffers);
    _aligned_free(alignedPtr);
    if (assetResult.error() != fastgltf::Error::None) {
        std::cerr << "Failed to parse GLTF: " << inputPath << "\n";
        return;
    }

    auto& asset = assetResult.get();

    std::cerr << "  Parsed OK. Meshes: " << asset.meshes.size()
              << ", Buffers: " << asset.buffers.size()
              << ", Accessors: " << asset.accessors.size() << "\n" << std::flush;

    // --- Extract vertex positions and indices for meshlet generation ---
    // We do this BEFORE buffer merging while accessors still point to valid data.
    std::vector<float> allPositions; // flat x,y,z
    std::vector<float> allNormals;   // flat x,y,z
    std::vector<uint32_t> allIndices;

    // Helper lambda to get raw buffer pointer from a fastgltf buffer
    auto getBufferBytes = [&](size_t bufferIndex) -> const std::byte* {
        auto& buf = asset.buffers[bufferIndex];
        if (auto* arr = std::get_if<fastgltf::sources::Array>(&buf.data)) {
            return arr->bytes.data();
        } else if (auto* vec = std::get_if<fastgltf::sources::Vector>(&buf.data)) {
            return vec->bytes.data();
        }
        return nullptr;
    };

    std::cerr << "  Extracting geometry from " << asset.meshes.size() << " meshes\n" << std::flush;

    for (const auto& mesh : asset.meshes) {
        for (const auto& prim : mesh.primitives) {
            uint32_t baseVertex = static_cast<uint32_t>(allPositions.size() / 3);

            auto posIt = prim.findAttribute("POSITION");
            if (posIt != prim.attributes.end()) {
                auto& posAccessor = asset.accessors[posIt->accessorIndex];
                size_t prevSize = allPositions.size();
                allPositions.resize(prevSize + posAccessor.count * 3);
                
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset, posAccessor, [&](fastgltf::math::fvec3 pos, std::size_t idx) {
                    allPositions[prevSize + idx * 3 + 0] = pos.x();
                    allPositions[prevSize + idx * 3 + 1] = pos.y();
                    allPositions[prevSize + idx * 3 + 2] = pos.z();
                });
            }
            
            auto normIt = prim.findAttribute("NORMAL");
            if (normIt != prim.attributes.end()) {
                auto& normAccessor = asset.accessors[normIt->accessorIndex];
                size_t prevSize = allNormals.size();
                allNormals.resize(prevSize + normAccessor.count * 3);
                
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset, normAccessor, [&](fastgltf::math::fvec3 norm, std::size_t idx) {
                    allNormals[prevSize + idx * 3 + 0] = norm.x();
                    allNormals[prevSize + idx * 3 + 1] = norm.y();
                    allNormals[prevSize + idx * 3 + 2] = norm.z();
                });
            } else if (posIt != prim.attributes.end()) {
                // Fill with zeros if no normals exist
                auto& posAccessor = asset.accessors[posIt->accessorIndex];
                allNormals.resize(allNormals.size() + posAccessor.count * 3, 0.0f);
            }

            if (prim.indicesAccessor.has_value()) {
                auto& idxAccessor = asset.accessors[prim.indicesAccessor.value()];
                size_t prevSize = allIndices.size();
                allIndices.resize(prevSize + idxAccessor.count);
                
                fastgltf::iterateAccessorWithIndex<uint32_t>(asset, idxAccessor, [&](uint32_t idx, std::size_t i) {
                    allIndices[prevSize + i] = idx + baseVertex;
                });
            } else if (posIt != prim.attributes.end()) {
                // Generate unindexed mesh indices
                auto& posAccessor = asset.accessors[posIt->accessorIndex];
                size_t prevSize = allIndices.size();
                allIndices.resize(prevSize + posAccessor.count);
                for(size_t i=0; i<posAccessor.count; ++i) {
                    allIndices[prevSize + i] = static_cast<uint32_t>(baseVertex + i);
                }
            }
        }
    }

    std::cerr << "  Extracted " << (allPositions.size() / 3) << " vertices, " << allIndices.size() << " indices\n" << std::flush;

    // --- Continue with texture embedding and GLB export ---
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

    if (inputPath != outputPath) {
        fastgltf::FileExporter exporter;
        auto error = exporter.writeGltfBinary(asset, outputPath);
        if (error != fastgltf::Error::None) {
            std::cerr << "Failed to export GLB: " << fastgltf::getErrorMessage(error) << "\n";
        } else {
            std::cout << "Successfully exported GLB to " << outputPath << "\n";
        }
    } else {
        std::cout << "Skipping GLB export, appending meshlets to existing file " << outputPath << "\n";
    }

    // --- Generate meshlets and append to GLB ---
    if (!allPositions.empty() && !allIndices.empty()) {
        normalize_winding_order(allPositions.data(), sizeof(float) * 3,
                                allNormals.empty() ? nullptr : allNormals.data(), sizeof(float) * 3,
                                allIndices.data(), allIndices.size());
                                
        generate_meshlets(allPositions.data(), sizeof(float) * 3,
                          allPositions.size() / 3,
                          allIndices.data(), allIndices.size(),
                          outputPath);
    }
}


struct Vertex {
    float pos[3];
    float color[3];
    float normal[3];
    float texCoord[2];
};

void compile_model(const std::string& inputPath, const std::string& outputPath) {
    std::cerr << "[compile_model] " << inputPath << "\n" << std::flush;
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
    if (inputPath != outputPath) {
        fastgltf::FileExporter exporter;
        auto error = exporter.writeGltfBinary(asset, outputPath);
        if (error != fastgltf::Error::None) {
            std::cerr << "Failed to export GLB: " << fastgltf::getErrorMessage(error) << "\n";
        } else {
            std::cout << "Successfully exported GLB to " << outputPath << "\n";
        }
    } else {
        std::cout << "Skipping GLB export, appending meshlets to existing file " << outputPath << "\n";
    }

    // Generate meshlets and append to GLB
    if (!vertices.empty() && !indices.empty()) {
        normalize_winding_order(vertices[0].pos, sizeof(Vertex),
                                vertices[0].normal, sizeof(Vertex),
                                indices.data(), indices.size());
                                
        // Vertex struct has pos at offset 0, stride = sizeof(Vertex)
        generate_meshlets(vertices[0].pos, sizeof(Vertex),
                          vertices.size(),
                          indices.data(), indices.size(),
                          outputPath);
    }
}

int main() {
    std::cerr << "[model_compiler] Starting...\n" << std::flush;
    compile_model("assets/models/obj/triangle.obj", "assets/models/glb/triangle.glb");
    compile_model("assets/models/obj/cube.obj", "assets/models/glb/cube.glb");
    compile_gltf_model("assets/models/obj/boulder_01_1k.gltf", "assets/models/glb/boulder_01_1k.glb");
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
                std::cerr << "[main] Compiling: " << entry.path().string() << "\n" << std::flush;
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
