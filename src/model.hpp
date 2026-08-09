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

struct Material {
  glm::vec4 baseColorFactor = glm::vec4(1.0f);
  float metallicFactor = 1.0f;
  float roughnessFactor = 1.0f;
  int albedoTexIndex = -1;
  int normalTexIndex = -1;
  int metallicRoughnessTexIndex = -1;
  int padding[3] = {0, 0, 0};
};

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

struct SubMesh {
  uint32_t indexOffset = 0;
  uint32_t indexCount = 0;
  uint32_t materialIndex = 0;
};

struct ModelData {
  uint32_t vertexCount = 0;
  VkBuffer vertexBuffer = VK_NULL_HANDLE;
  VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;

  uint32_t indexCount = 0;
  VkBuffer indexBuffer = VK_NULL_HANDLE;
  VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
  bool hasIndexBuffer = false;

  uint32_t meshletCount = 0;
  VkBuffer meshletBuffer = VK_NULL_HANDLE;
  VkDeviceMemory meshletBufferMemory = VK_NULL_HANDLE;
  
  VkBuffer meshletVerticesBuffer = VK_NULL_HANDLE;
  VkDeviceMemory meshletVerticesBufferMemory = VK_NULL_HANDLE;
  
  VkBuffer meshletTrianglesBuffer = VK_NULL_HANDLE;
  VkDeviceMemory meshletTrianglesBufferMemory = VK_NULL_HANDLE;
  
  VkBuffer meshletBoundsBuffer = VK_NULL_HANDLE;
  VkDeviceMemory meshletBoundsBufferMemory = VK_NULL_HANDLE;

  VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

  std::vector<std::vector<uint8_t>> rawImages;
  
  std::vector<SubMesh> subMeshes;
  std::vector<Material> materials;
};

namespace model {
void create(DeviceState &deviceState, const std::vector<Vertex> &vertices,
            const std::vector<uint32_t> &indices, ModelData &outModel);
void destroy(DeviceState &deviceState, ModelData &model);
void bind(VkCommandBuffer commandBuffer, const ModelData &model);
void draw(VkCommandBuffer commandBuffer, const ModelData &model);

bool load_glb(DeviceState &deviceState, const std::string &filepath, ModelData &outModel);
} // namespace model

} // namespace vke
