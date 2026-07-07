#pragma once
#include "device.hpp"
#include <string>
#include <vulkan/vulkan.h>

namespace vke {

struct TextureData {
  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory imageMemory = VK_NULL_HANDLE;
  VkImageView imageView = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;
  VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t mipLevels = 1;
};

namespace texture {
bool load_ktx2(DeviceState &deviceState, const std::string &filepath,
               TextureData &outTexture);
bool load_ktx2_from_memory(DeviceState &deviceState, const uint8_t* data, size_t size,
                           TextureData &outTexture);
bool load_image_from_memory(DeviceState &deviceState, const uint8_t* data, size_t size,
                            TextureData &outTexture);
bool create_fallback(DeviceState &deviceState, TextureData &outTexture);
void destroy(DeviceState &deviceState, TextureData &texture);
} // namespace texture

} // namespace vke
