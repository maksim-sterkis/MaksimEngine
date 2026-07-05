#include "asset_pool.hpp"

namespace vke::asset {

void init_default_texture(AssetPool &pool, DeviceState &deviceState,
                          VkDescriptorPool descriptorPool,
                          VkDescriptorSetLayout descriptorSetLayout) {
  if (pool.default_texture_handle != 0)
    return;

  TextureData tex;
  if (!texture::create_fallback(deviceState, tex)) {
    throw std::runtime_error("failed to create fallback texture!");
  }

  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = descriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &descriptorSetLayout;

  if (vkAllocateDescriptorSets(deviceState.device, &allocInfo,
                               &tex.descriptorSet) != VK_SUCCESS) {
    throw std::runtime_error(
        "failed to allocate descriptor set for fallback texture!");
  }

  VkDescriptorImageInfo imageInfo{};
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfo.imageView = tex.imageView;
  imageInfo.sampler = tex.sampler;

  VkWriteDescriptorSet descriptorWrite{};
  descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet = tex.descriptorSet;
  descriptorWrite.dstBinding = 0;
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pImageInfo = &imageInfo;

  vkUpdateDescriptorSets(deviceState.device, 1, &descriptorWrite, 0, nullptr);

  pool.textures.push_back(tex);
  pool.default_texture_handle = static_cast<uint32_t>(pool.textures.size());
}

uint32_t load_model(AssetPool &pool, DeviceState &deviceState,
                    const std::string &filepath, bool force_palette_colors) {
  if (pool.model_registry.find(filepath) != pool.model_registry.end()) {
    return pool.model_registry[filepath];
  }

  ModelData model;
  if (model::load_obj(deviceState, filepath, force_palette_colors, model)) {
    pool.models.push_back(model);
    uint32_t handle = static_cast<uint32_t>(pool.models.size());
    pool.model_registry[filepath] = handle;
    return handle;
  }
  return static_cast<uint32_t>(-1);
}

const ModelData *get_model(const AssetPool &pool, uint32_t handle) {
  if (handle == 0 || handle > pool.models.size())
    return nullptr;
  return &pool.models[handle - 1];
}

uint32_t load_texture(AssetPool &pool, DeviceState &deviceState,
                      VkDescriptorPool descriptorPool,
                      VkDescriptorSetLayout descriptorSetLayout,
                      const std::string &filepath) {
  if (pool.texture_registry.contains(filepath)) {
    return pool.texture_registry[filepath];
  }

  TextureData tex;
  if (!texture::load_ktx2(deviceState, filepath, tex)) {
    return 0;
  }

  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = descriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &descriptorSetLayout;

  if (vkAllocateDescriptorSets(deviceState.device, &allocInfo,
                               &tex.descriptorSet) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate descriptor set for texture!");
  }

  VkDescriptorImageInfo imageInfo{};
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfo.imageView = tex.imageView;
  imageInfo.sampler = tex.sampler;

  VkWriteDescriptorSet descriptorWrite{};
  descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet = tex.descriptorSet;
  descriptorWrite.dstBinding = 0;
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pImageInfo = &imageInfo;

  vkUpdateDescriptorSets(deviceState.device, 1, &descriptorWrite, 0, nullptr);

  pool.textures.push_back(tex);
  uint32_t handle = static_cast<uint32_t>(pool.textures.size());
  pool.texture_registry[filepath] = handle;
  return handle;
}

const TextureData *get_texture(const AssetPool &pool, uint32_t handle) {
  if (handle == 0 || handle > pool.textures.size())
    return nullptr;
  return &pool.textures[handle - 1];
}

void cleanup(AssetPool &pool, DeviceState &deviceState) {
  for (auto &model : pool.models) {
    model::destroy(deviceState, model);
  }
  pool.models.clear();
  pool.model_registry.clear();

  for (auto &tex : pool.textures) {
    texture::destroy(deviceState, tex);
  }
  pool.textures.clear();
  pool.texture_registry.clear();
}

} // namespace vke::asset
