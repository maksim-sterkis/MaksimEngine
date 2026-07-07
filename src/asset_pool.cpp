#include "asset_pool.hpp"
#include <iostream>
#include <cstring>

namespace vke::asset {

void init_default_texture(AssetPool &pool, DeviceState &deviceState,
                          VkDescriptorSet globalDescriptorSet) {
  if (pool.default_texture_handle != 0)
    return;

  TextureData tex;
  if (!texture::create_fallback(deviceState, tex)) {
    throw std::runtime_error("failed to create fallback texture!");
  }
  tex.descriptorSet = globalDescriptorSet; // Storing the global set for convenience if needed

  pool.textures.push_back(tex);
  pool.default_texture_handle = static_cast<uint32_t>(pool.textures.size());
  pool.texture_registry["fallback"] = pool.default_texture_handle;

  VkDescriptorImageInfo imageInfo{};
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfo.imageView = tex.imageView;
  imageInfo.sampler = tex.sampler;

  VkWriteDescriptorSet descriptorWrite{};
  descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet = globalDescriptorSet;
  descriptorWrite.dstBinding = 0;
  descriptorWrite.dstArrayElement = pool.default_texture_handle;
  descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pImageInfo = &imageInfo;

  vkUpdateDescriptorSets(deviceState.device, 1, &descriptorWrite, 0, nullptr);
}

uint32_t load_model(AssetPool &pool, DeviceState &deviceState,
                    VkDescriptorSet globalDescriptorSet,
                    const std::string &filepath) {
  if (pool.model_registry.find(filepath) != pool.model_registry.end()) {
    return pool.model_registry[filepath];
  }

  ModelData model;
  if (model::load_glb(deviceState, filepath, model)) {
    for (const auto& imgBytes : model.rawImages) {
        TextureData tex;
        bool isKtx = false;
        if (imgBytes.size() >= 12) {
            const uint8_t ktxMagic[] = {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};
            if (std::memcmp(imgBytes.data(), ktxMagic, 12) == 0) {
                isKtx = true;
            }
        }
        
        bool loaded = false;
        if (isKtx) {
            loaded = texture::load_ktx2_from_memory(deviceState, imgBytes.data(), imgBytes.size(), tex);
        } else {
            loaded = texture::load_image_from_memory(deviceState, imgBytes.data(), imgBytes.size(), tex);
        }

        if (loaded) {
            tex.descriptorSet = globalDescriptorSet;
            pool.textures.push_back(tex);
            uint32_t handle = static_cast<uint32_t>(pool.textures.size());

            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = tex.imageView;
            imageInfo.sampler = tex.sampler;

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = globalDescriptorSet;
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = handle;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pImageInfo = &imageInfo;

            vkUpdateDescriptorSets(deviceState.device, 1, &descriptorWrite, 0, nullptr);
        }
    }
    
    pool.models.push_back(model);
    uint32_t handle = static_cast<uint32_t>(pool.models.size());
    pool.model_registry[filepath] = handle;
    return handle;
  }
  return static_cast<uint32_t>(-1);
}

const ModelData *get_model(const AssetPool &pool, uint32_t handle) {
  if (handle == 0 || handle > pool.models.size()) {
    return nullptr;
  }
  return &pool.models[handle - 1];
}

uint32_t load_texture(AssetPool &pool, DeviceState &deviceState,
                      VkDescriptorSet globalDescriptorSet,
                      const std::string &filepath) {
  if (pool.texture_registry.contains(filepath)) {
    return pool.texture_registry[filepath];
  }

  TextureData tex;
  if (!texture::load_ktx2(deviceState, filepath, tex)) {
    return 0;
  }
  tex.descriptorSet = globalDescriptorSet;

  pool.textures.push_back(tex);
  uint32_t handle = static_cast<uint32_t>(pool.textures.size());
  pool.texture_registry[filepath] = handle;

  VkDescriptorImageInfo imageInfo{};
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfo.imageView = tex.imageView;
  imageInfo.sampler = tex.sampler;

  VkWriteDescriptorSet descriptorWrite{};
  descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet = globalDescriptorSet;
  descriptorWrite.dstBinding = 0;
  descriptorWrite.dstArrayElement = handle;
  descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pImageInfo = &imageInfo;

  vkUpdateDescriptorSets(deviceState.device, 1, &descriptorWrite, 0, nullptr);

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
