#pragma once
#include "device.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace vke {

struct Vertex {
  glm::vec3 position;
  glm::vec3 color;
  glm::vec3 normal;
  glm::vec2 uv;

  bool operator==(const Vertex &other) const {
    return position == other.position && color == other.color &&
           normal == other.normal && uv == other.uv;
  }

  static VkVertexInputBindingDescription getBindingDescription();
  static std::vector<VkVertexInputAttributeDescription>
  getAttributeDescriptions();
};

} // namespace vke

namespace std {
template <> struct hash<vke::Vertex> {
  size_t operator()(const vke::Vertex &vertex) const {
    auto hash_combine = [](size_t &seed, float v) {
      std::hash<float> hasher;
      seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    };
    size_t seed = 0;
    hash_combine(seed, vertex.position.x);
    hash_combine(seed, vertex.position.y);
    hash_combine(seed, vertex.position.z);
    hash_combine(seed, vertex.color.x);
    hash_combine(seed, vertex.color.y);
    hash_combine(seed, vertex.color.z);
    hash_combine(seed, vertex.normal.x);
    hash_combine(seed, vertex.normal.y);
    hash_combine(seed, vertex.normal.z);
    hash_combine(seed, vertex.uv.x);
    hash_combine(seed, vertex.uv.y);
    return seed;
  }
};
} // namespace std

namespace vke {

struct ModelData {
  uint32_t vertexCount = 0;
  VkBuffer vertexBuffer = VK_NULL_HANDLE;
  VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;

  uint32_t indexCount = 0;
  VkBuffer indexBuffer = VK_NULL_HANDLE;
  VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
  bool hasIndexBuffer = false;
};

namespace model {
void create(DeviceState &deviceState, const std::vector<Vertex> &vertices,
            const std::vector<uint32_t> &indices, ModelData &outModel);
void destroy(DeviceState &deviceState, ModelData &model);
void bind(VkCommandBuffer commandBuffer, const ModelData &model);
void draw(VkCommandBuffer commandBuffer, const ModelData &model);

// force_palette_colors will override the tinyobjloader default white colors
// with a face palette
bool load_obj(DeviceState &deviceState, const std::string &filepath,
              bool force_palette_colors, ModelData &outModel);
} // namespace model

} // namespace vke
