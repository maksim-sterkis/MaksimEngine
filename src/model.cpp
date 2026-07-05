#include "model.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <tiny_obj_loader.h>
#include <unordered_map>


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
  VkDeviceSize vertexBufferSize = sizeof(vertices[0]) * outModel.vertexCount;

  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  device::create_buffer(deviceState, vertexBufferSize,
                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        stagingBuffer, stagingBufferMemory);

  void *data;
  vkMapMemory(deviceState.device, stagingBufferMemory, 0, vertexBufferSize, 0,
              &data);
  memcpy(data, vertices.data(), (size_t)vertexBufferSize);
  vkUnmapMemory(deviceState.device, stagingBufferMemory);

  device::create_buffer(deviceState, vertexBufferSize,
                        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        outModel.vertexBuffer, outModel.vertexBufferMemory);

  device::copy_buffer(deviceState, stagingBuffer, outModel.vertexBuffer,
                      vertexBufferSize);

  vkDestroyBuffer(deviceState.device, stagingBuffer, nullptr);
  vkFreeMemory(deviceState.device, stagingBufferMemory, nullptr);

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

bool load_obj(DeviceState &deviceState, const std::string &filepath,
              bool force_palette_colors, ModelData &outModel) {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warn, err;

  if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                        filepath.c_str())) {
    std::cerr << "failed to open OBJ file: " << filepath << " Error: " << warn
              << err << "\n";
    return false;
  }

  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  std::unordered_map<Vertex, uint32_t> uniqueVertices;

  // Palette of colors for the faces
  glm::vec3 faceColors[] = {
      {1.0f, 0.0f, 0.0f}, // Red
      {0.0f, 1.0f, 0.0f}, // Green
      {0.0f, 0.0f, 1.0f}, // Blue
      {1.0f, 1.0f, 0.0f}, // Yellow
      {1.0f, 0.0f, 1.0f}, // Magenta
      {0.0f, 1.0f, 1.0f}  // Cyan
  };
  int vertexIndex = 0;

  for (const auto &shape : shapes) {
    for (size_t i = 0; i < shape.mesh.indices.size(); i += 3) {
      Vertex v[3];
      for (int j = 0; j < 3; j++) {
        auto index = shape.mesh.indices[i + j];

        v[j].position = {attrib.vertices[3 * index.vertex_index + 0],
                         attrib.vertices[3 * index.vertex_index + 1],
                         attrib.vertices[3 * index.vertex_index + 2]};

        if (index.texcoord_index >= 0) {
          v[j].uv = {
              attrib.texcoords[2 * index.texcoord_index + 0],
              1.0f - attrib.texcoords[2 * index.texcoord_index +
                                      1] // Vulkan flips Y
          };
        } else {
          v[j].uv = {0.0f, 0.0f};
        }

        if (index.normal_index >= 0) {
          v[j].normal = {attrib.normals[3 * index.normal_index + 0],
                         attrib.normals[3 * index.normal_index + 1],
                         attrib.normals[3 * index.normal_index + 2]};
        }

        if (!force_palette_colors &&
            attrib.colors.size() > 3 * index.vertex_index + 2) {
          v[j].color = {attrib.colors[3 * index.vertex_index + 0],
                        attrib.colors[3 * index.vertex_index + 1],
                        attrib.colors[3 * index.vertex_index + 2]};
        } else {
          v[j].color = faceColors[(vertexIndex / 3) % 6];
        }
        vertexIndex++;
      }

      // Compute flat normal if missing
      if (shape.mesh.indices[i].normal_index < 0) {
        glm::vec3 e1 = v[1].position - v[0].position;
        glm::vec3 e2 = v[2].position - v[0].position;
        glm::vec3 normal = glm::normalize(glm::cross(e1, e2));
        v[0].normal = normal;
        v[1].normal = normal;
        v[2].normal = normal;
      }

      for (int j = 0; j < 3; j++) {
        if (uniqueVertices.count(v[j]) == 0) {
          uniqueVertices[v[j]] = static_cast<uint32_t>(vertices.size());
          vertices.push_back(v[j]);
        }
        indices.push_back(uniqueVertices[v[j]]);
      }
    }
  }

  create(deviceState, vertices, indices, outModel);
  return true;
}

} // namespace model
} // namespace vke
