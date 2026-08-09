#include "model.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <filesystem>

namespace vke {

VkVertexInputBindingDescription Vertex::getBindingDescription() {
  VkVertexInputBindingDescription bindingDescription{};
  bindingDescription.binding = 0;
  bindingDescription.stride = sizeof(Vertex);
  bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription>
Vertex::getAttributeDescriptions() {
  std::vector<VkVertexInputAttributeDescription> attributeDescriptions(4);

  attributeDescriptions[0].binding = 0;
  attributeDescriptions[0].location = 0;
  attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  attributeDescriptions[0].offset = offsetof(Vertex, position);

  attributeDescriptions[1].binding = 0;
  attributeDescriptions[1].location = 1;
  attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
  attributeDescriptions[1].offset = offsetof(Vertex, color);

  attributeDescriptions[2].binding = 0;
  attributeDescriptions[2].location = 2;
  attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
  attributeDescriptions[2].offset = offsetof(Vertex, normal);

  attributeDescriptions[3].binding = 0;
  attributeDescriptions[3].location = 3;
  attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
  attributeDescriptions[3].offset = offsetof(Vertex, uv);

  return attributeDescriptions;
}

namespace model {

void create(DeviceState &deviceState, const std::vector<Vertex> &vertices,
            const std::vector<uint32_t> &indices, ModelData &outModel) {
  // 1. Vertex Buffer
  outModel.vertexCount = static_cast<uint32_t>(vertices.size());
  if (!vertices.empty()) {
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    device::create_buffer(deviceState, bufferSize,
                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          stagingBuffer, stagingBufferMemory);

    void *data;
    vkMapMemory(deviceState.device, stagingBufferMemory, 0, bufferSize, 0,
                &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(deviceState.device, stagingBufferMemory);

    device::create_buffer(deviceState, bufferSize,
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                          outModel.vertexBuffer, outModel.vertexBufferMemory);

    device::copy_buffer(deviceState, stagingBuffer, outModel.vertexBuffer,
                        bufferSize);

    vkDestroyBuffer(deviceState.device, stagingBuffer, nullptr);
    vkFreeMemory(deviceState.device, stagingBufferMemory, nullptr);
  }

  // 2. Index Buffer
  outModel.indexCount = static_cast<uint32_t>(indices.size());
  if (outModel.indexCount > 0) {
    VkDeviceSize indexBufferSize = sizeof(indices[0]) * outModel.indexCount;

    VkBuffer indexStagingBuffer;
    VkDeviceMemory indexStagingBufferMemory;
    device::create_buffer(deviceState, indexBufferSize,
                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          indexStagingBuffer, indexStagingBufferMemory);

    void *indexData;
    vkMapMemory(deviceState.device, indexStagingBufferMemory, 0,
                indexBufferSize, 0, &indexData);
    memcpy(indexData, indices.data(), (size_t)indexBufferSize);
    vkUnmapMemory(deviceState.device, indexStagingBufferMemory);

    device::create_buffer(deviceState, indexBufferSize,
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                              VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                          outModel.indexBuffer, outModel.indexBufferMemory);

    device::copy_buffer(deviceState, indexStagingBuffer, outModel.indexBuffer,
                        indexBufferSize);

    vkDestroyBuffer(deviceState.device, indexStagingBuffer, nullptr);
    vkFreeMemory(deviceState.device, indexStagingBufferMemory, nullptr);
    outModel.hasIndexBuffer = true;
  } else {
    outModel.indexBuffer = VK_NULL_HANDLE;
    outModel.indexBufferMemory = VK_NULL_HANDLE;
    outModel.hasIndexBuffer = false;
  }
}

void destroy(DeviceState &deviceState, ModelData &model) {
  if (model.vertexBuffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(deviceState.device, model.vertexBuffer, nullptr);
    model.vertexBuffer = VK_NULL_HANDLE;
  }
  if (model.vertexBufferMemory != VK_NULL_HANDLE) {
    vkFreeMemory(deviceState.device, model.vertexBufferMemory, nullptr);
    model.vertexBufferMemory = VK_NULL_HANDLE;
  }
  model.vertexCount = 0;

  if (model.indexBuffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(deviceState.device, model.indexBuffer, nullptr);
    model.indexBuffer = VK_NULL_HANDLE;
  }
  if (model.indexBufferMemory != VK_NULL_HANDLE) {
    vkFreeMemory(deviceState.device, model.indexBufferMemory, nullptr);
    model.indexBufferMemory = VK_NULL_HANDLE;
  }
  model.indexCount = 0;
  model.hasIndexBuffer = false;

  auto destroyBuf = [&](VkBuffer& b, VkDeviceMemory& m) {
    if (b != VK_NULL_HANDLE) { vkDestroyBuffer(deviceState.device, b, nullptr); b = VK_NULL_HANDLE; }
    if (m != VK_NULL_HANDLE) { vkFreeMemory(deviceState.device, m, nullptr); m = VK_NULL_HANDLE; }
  };
  destroyBuf(model.meshletBuffer, model.meshletBufferMemory);
  destroyBuf(model.meshletVerticesBuffer, model.meshletVerticesBufferMemory);
  destroyBuf(model.meshletTrianglesBuffer, model.meshletTrianglesBufferMemory);
  destroyBuf(model.meshletBoundsBuffer, model.meshletBoundsBufferMemory);
  model.meshletCount = 0;
}

void bind(VkCommandBuffer commandBuffer, const ModelData &model) {
  VkBuffer buffers[] = {model.vertexBuffer};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
  if (model.hasIndexBuffer) {
    vkCmdBindIndexBuffer(commandBuffer, model.indexBuffer, 0,
                         VK_INDEX_TYPE_UINT32);
  }
}

void draw(VkCommandBuffer commandBuffer, const ModelData &model) {
  if (model.hasIndexBuffer) {
    vkCmdDrawIndexed(commandBuffer, model.indexCount, 1, 0, 0, 0);
  } else {
    vkCmdDraw(commandBuffer, model.vertexCount, 1, 0, 0);
  }
}

bool load_glb(DeviceState &deviceState, const std::string &filepath, ModelData &outModel) {
  fastgltf::Parser parser(fastgltf::Extensions::KHR_texture_basisu);
  auto data = fastgltf::GltfDataBuffer::FromPath(std::filesystem::path(filepath));
  if (data.error() != fastgltf::Error::None) {
    std::cerr << "Failed to load GLB: " << filepath << " Error: " << fastgltf::getErrorMessage(data.error()) << "\n";
    return false;
  }

  auto asset = parser.loadGltfBinary(data.get(), std::filesystem::path(filepath).parent_path(), fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers);
  if (asset.error() != fastgltf::Error::None) {
    std::cerr << "Failed to parse GLB: " << filepath << " Error: " << fastgltf::getErrorMessage(asset.error()) << "\n";
    return false;
  }

  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;

  for (const auto& mesh : asset.get().meshes) {
    for (const auto& prim : mesh.primitives) {
      auto initialVertex = vertices.size();

      auto posIt = prim.findAttribute("POSITION");
      if (posIt != prim.attributes.end()) {
        auto& posAccessor = asset.get().accessors[posIt->accessorIndex];
        vertices.resize(initialVertex + posAccessor.count);
        
        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset.get(), posAccessor, [&](fastgltf::math::fvec3 pos, std::size_t idx) {
          vertices[initialVertex + idx].position = {pos.x(), pos.y(), pos.z()};
          vertices[initialVertex + idx].color = {1.0f, 1.0f, 1.0f};
          vertices[initialVertex + idx].normal = {1.0f, 0.0f, 0.0f};
          vertices[initialVertex + idx].uv = {0.0f, 0.0f};
        });
      }

      auto normIt = prim.findAttribute("NORMAL");
      if (normIt != prim.attributes.end()) {
        auto& normAccessor = asset.get().accessors[normIt->accessorIndex];
        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset.get(), normAccessor, [&](fastgltf::math::fvec3 norm, std::size_t idx) {
          vertices[initialVertex + idx].normal = {norm.x(), norm.y(), norm.z()};
        });
      }

      auto colIt = prim.findAttribute("COLOR_0");
      if (colIt != prim.attributes.end()) {
        auto& colAccessor = asset.get().accessors[colIt->accessorIndex];
        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset.get(), colAccessor, [&](fastgltf::math::fvec3 col, std::size_t idx) {
          vertices[initialVertex + idx].color = {col.x(), col.y(), col.z()};
        });
      }

      auto uvIt = prim.findAttribute("TEXCOORD_0");
      if (uvIt != prim.attributes.end()) {
        auto& uvAccessor = asset.get().accessors[uvIt->accessorIndex];
        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(asset.get(), uvAccessor, [&](fastgltf::math::fvec2 uv, std::size_t idx) {
          vertices[initialVertex + idx].uv = {uv.x(), uv.y()};
        });
      }

      if (prim.indicesAccessor.has_value()) {
        auto initialIndex = indices.size();
        auto& idxAccessor = asset.get().accessors[prim.indicesAccessor.value()];
        indices.resize(initialIndex + idxAccessor.count);
        fastgltf::iterateAccessorWithIndex<uint32_t>(asset.get(), idxAccessor, [&](uint32_t idx, std::size_t i) {
          indices[initialIndex + i] = idx + initialVertex;
        });
      }
    }
  }

  create(deviceState, vertices, indices, outModel);

  for (const auto& mat : asset.get().materials) {
      Material outMat;
      if (mat.pbrData.baseColorTexture.has_value()) {
          auto texIdx = mat.pbrData.baseColorTexture->textureIndex;
          outMat.albedoTexIndex = asset.get().textures[texIdx].imageIndex.value_or(-1);
      }
      if (mat.normalTexture.has_value()) {
          auto texIdx = mat.normalTexture->textureIndex;
          outMat.normalTexIndex = asset.get().textures[texIdx].imageIndex.value_or(-1);
      }
      if (mat.pbrData.metallicRoughnessTexture.has_value()) {
          auto texIdx = mat.pbrData.metallicRoughnessTexture->textureIndex;
          outMat.metallicRoughnessTexIndex = asset.get().textures[texIdx].imageIndex.value_or(-1);
      }
      
      outMat.baseColorFactor = glm::vec4(mat.pbrData.baseColorFactor[0], mat.pbrData.baseColorFactor[1], mat.pbrData.baseColorFactor[2], mat.pbrData.baseColorFactor[3]);
      outMat.metallicFactor = mat.pbrData.metallicFactor;
      outMat.roughnessFactor = mat.pbrData.roughnessFactor;
      
      outModel.materials.push_back(outMat);
  }

  if (outModel.materials.empty()) {
      outModel.materials.push_back(Material{});
  }

  uint32_t currentSubMeshIndexOffset = 0;
  for (const auto& mesh : asset.get().meshes) {
      for (const auto& prim : mesh.primitives) {
          SubMesh sub;
          sub.materialIndex = prim.materialIndex.value_or(0);
          if (prim.indicesAccessor.has_value()) {
              auto& idxAccessor = asset.get().accessors[prim.indicesAccessor.value()];
              sub.indexCount = idxAccessor.count;
          } else {
              auto posIt = prim.findAttribute("POSITION");
              if (posIt != prim.attributes.end()) {
                  auto& posAccessor = asset.get().accessors[posIt->accessorIndex];
                  sub.indexCount = posAccessor.count;
              }
          }
          sub.indexOffset = currentSubMeshIndexOffset;
          currentSubMeshIndexOffset += sub.indexCount;
          outModel.subMeshes.push_back(sub);
      }
  }

  for (const auto& image : asset.get().images) {
    std::vector<uint8_t> imgBytes;
    std::visit(fastgltf::visitor{
      [&](const fastgltf::sources::BufferView& view) {
        auto& bufferView = asset.get().bufferViews[view.bufferViewIndex];
        auto& buffer = asset.get().buffers[bufferView.bufferIndex];
        std::visit(fastgltf::visitor{
          [&](auto& data) {
            if constexpr (requires { data.bytes; }) {
                auto* ptr = reinterpret_cast<const uint8_t*>(data.bytes.data());
                imgBytes.assign(ptr + bufferView.byteOffset, 
                                ptr + bufferView.byteOffset + bufferView.byteLength);
            } else {
                std::cerr << "DEBUG: Unhandled buffer data variant! index=" << buffer.data.index() << "\n";
            }
          }
        }, buffer.data);
      },
      [&](auto& data) {
        if constexpr (requires { data.bytes; }) {
            auto* ptr = reinterpret_cast<const uint8_t*>(data.bytes.data());
            imgBytes.assign(ptr, ptr + data.bytes.size());
        } else {
            std::cerr << "DEBUG: Unhandled image source variant! index=" << image.data.index() << "\n";
        }
      }
    }, image.data);
    
    if (imgBytes.empty()) {
        std::cerr << "DEBUG: imgBytes is empty for image " << image.name << " (variant index " << image.data.index() << ")\n";
    }
    
    outModel.rawImages.push_back(std::move(imgBytes));
  }

  // --- Meshlet Payload Loading ---
  std::ifstream file(filepath, std::ios::binary | std::ios::ate);
  if (file.is_open()) {
    std::streamsize fileSize = file.tellg();
    if (fileSize > 12) {
      file.seekg(-12, std::ios::end);
      uint64_t payloadSize = 0;
      char magic[5] = {0};
      file.read(reinterpret_cast<char*>(&payloadSize), 8);
      file.read(magic, 4);

      if (std::string(magic) == "MESH") {
        file.seekg(-(12 + static_cast<std::streamoff>(payloadSize)), std::ios::end);
        std::vector<char> payload(payloadSize);
        file.read(payload.data(), payloadSize);

        MeshletFileHeader header;
        std::memcpy(&header, payload.data(), sizeof(MeshletFileHeader));

        if (header.magic == 0x4853454D) {
          outModel.meshletCount = header.meshletCount;
          
          size_t meshletSize = header.meshletCount * 16; // meshopt_Meshlet is 16 bytes
          size_t verticesSize = header.meshletVertexCount * 4; // uint32
          size_t trianglesSize = header.meshletTriangleCount * 1; // uint8
          size_t boundsSize = header.meshletCount * sizeof(MeshletBoundsGPU);
          
          size_t offset = sizeof(MeshletFileHeader);
          
          // Helper to create and copy buffer
          auto uploadToBuffer = [&](VkBuffer& buf, VkDeviceMemory& mem, size_t sz, const void* pData) {
            if (sz == 0) return;
            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;
            device::create_buffer(deviceState, sz, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
            void* mapped;
            vkMapMemory(deviceState.device, stagingBufferMemory, 0, sz, 0, &mapped);
            std::memcpy(mapped, pData, sz);
            vkUnmapMemory(deviceState.device, stagingBufferMemory);
            
            device::create_buffer(deviceState, sz, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buf, mem);
            device::copy_buffer(deviceState, stagingBuffer, buf, sz);
            
            vkDestroyBuffer(deviceState.device, stagingBuffer, nullptr);
            vkFreeMemory(deviceState.device, stagingBufferMemory, nullptr);
          };

          uploadToBuffer(outModel.meshletBuffer, outModel.meshletBufferMemory, meshletSize, payload.data() + offset);
          offset += meshletSize;
          uploadToBuffer(outModel.meshletVerticesBuffer, outModel.meshletVerticesBufferMemory, verticesSize, payload.data() + offset);
          offset += verticesSize;
          uploadToBuffer(outModel.meshletTrianglesBuffer, outModel.meshletTrianglesBufferMemory, trianglesSize, payload.data() + offset);
          offset += trianglesSize;
          uploadToBuffer(outModel.meshletBoundsBuffer, outModel.meshletBoundsBufferMemory, boundsSize, payload.data() + offset);
          
          std::cout << "Successfully loaded " << header.meshletCount << " meshlets for " << filepath << "\n";
        }
      }
    }
  }

  // Handle missing meshlets (e.g. if the tool failed to compile them)
  if (outModel.meshletCount == 0) {
      std::cerr << "Warning: No meshlets found in " << filepath << ". Creating empty buffers.\n";
      // Create empty 4-byte buffers to keep Vulkan validation happy
      VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      device::create_buffer(deviceState, 4, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, outModel.meshletBuffer, outModel.meshletBufferMemory);
      device::create_buffer(deviceState, 4, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, outModel.meshletVerticesBuffer, outModel.meshletVerticesBufferMemory);
      device::create_buffer(deviceState, 4, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, outModel.meshletTrianglesBuffer, outModel.meshletTrianglesBufferMemory);
      device::create_buffer(deviceState, 4, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, outModel.meshletBoundsBuffer, outModel.meshletBoundsBufferMemory);
  }

  return true;
}

} // namespace model
} // namespace vke
