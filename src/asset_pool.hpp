#pragma once

#include "device.hpp"
#include "model.hpp"
#include "texture.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace vke {

struct AssetPool {
  std::vector<ModelData> models;
  std::unordered_map<std::string, uint32_t> model_registry;

  std::vector<TextureData> textures;
  std::unordered_map<std::string, uint32_t> texture_registry;
  uint32_t default_texture_handle = 0;
};

namespace asset {
// Load a model from a file and return its handle. If it already exists, return
// existing handle.
void init_default_texture(AssetPool &pool, DeviceState &deviceState,
                          VkDescriptorSet globalDescriptorSet);

uint32_t load_model(AssetPool &pool, DeviceState &deviceState,
                    VkDescriptorSet globalDescriptorSet,
                    const std::string &filepath);

// Get the ModelData by handle
const ModelData *get_model(const AssetPool &pool, uint32_t handle);

uint32_t load_texture(AssetPool &pool, DeviceState &deviceState,
                      VkDescriptorSet globalDescriptorSet,
                      const std::string &filepath);
const TextureData *get_texture(const AssetPool &pool, uint32_t handle);

void cleanup(AssetPool &pool, DeviceState &deviceState);
} // namespace asset

} // namespace vke
