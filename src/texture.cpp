#include "texture.hpp"
#include <iostream>
#include <ktx.h>
#include <ktxvulkan.h>

namespace vke::texture {

bool load_ktx2(DeviceState &deviceState, const std::string &filepath,
               TextureData &outTexture) {
  ktxTexture2 *kTexture;
  KTX_error_code result;

  result = ktxTexture2_CreateFromNamedFile(
      filepath.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &kTexture);
  if (result != KTX_SUCCESS) {
    std::cerr << "Failed to load KTX2 texture: " << filepath
              << " Error: " << ktxErrorString(result) << std::endl;
    return false;
  }

  if (ktxTexture2_NeedsTranscoding(kTexture)) {
    result = ktxTexture2_TranscodeBasis(kTexture, KTX_TTF_BC7_RGBA, 0);
    if (result != KTX_SUCCESS) {
      std::cerr << "Failed to transcode KTX2 basis texture: " << filepath
                << std::endl;
      ktxTexture_Destroy(ktxTexture(kTexture));
      return false;
    }
  }

  ktxVulkanDeviceInfo vdi;
  ktxVulkanDeviceInfo_Construct(&vdi, deviceState.physicalDevice,
                                deviceState.device, deviceState.graphicsQueue,
                                deviceState.commandPool, nullptr);

  ktxVulkanTexture ktxVulkanTex;
  result = ktxTexture_VkUploadEx(
      ktxTexture(kTexture), &vdi, &ktxVulkanTex, VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  if (result != KTX_SUCCESS) {
    std::cerr << "Failed to upload KTX2 texture: " << filepath << std::endl;
    ktxTexture_Destroy(ktxTexture(kTexture));
    ktxVulkanDeviceInfo_Destruct(&vdi);
    return false;
  }

  outTexture.image = ktxVulkanTex.image;
  outTexture.imageMemory = ktxVulkanTex.deviceMemory;
  outTexture.width = ktxVulkanTex.width;
  outTexture.height = ktxVulkanTex.height;
  outTexture.mipLevels = ktxVulkanTex.levelCount;

  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = outTexture.image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = ktxVulkanTex.imageFormat;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = outTexture.mipLevels;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  if (vkCreateImageView(deviceState.device, &viewInfo, nullptr,
                        &outTexture.imageView) != VK_SUCCESS) {
    std::cerr << "Failed to create texture image view!" << std::endl;
  }

  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.anisotropyEnable = VK_FALSE;
  samplerInfo.maxAnisotropy = 1.0f;
  samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.mipLodBias = 0.0f;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = static_cast<float>(outTexture.mipLevels);

  if (vkCreateSampler(deviceState.device, &samplerInfo, nullptr,
                      &outTexture.sampler) != VK_SUCCESS) {
    std::cerr << "Failed to create texture sampler!" << std::endl;
  }

  ktxTexture_Destroy(ktxTexture(kTexture));
  ktxVulkanDeviceInfo_Destruct(&vdi);

  return true;
}

bool create_fallback(DeviceState &deviceState, TextureData &outTexture) {
  ktxTexture2 *ktxTex;
  ktxTextureCreateInfo createInfo{};
  createInfo.vkFormat = VK_FORMAT_R8G8B8A8_UNORM;
  createInfo.baseWidth = 1;
  createInfo.baseHeight = 1;
  createInfo.baseDepth = 1;
  createInfo.numDimensions = 2;
  createInfo.numLevels = 1;
  createInfo.numLayers = 1;
  createInfo.numFaces = 1;
  createInfo.generateMipmaps = KTX_FALSE;

  if (ktxTexture2_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE,
                         &ktxTex) != KTX_SUCCESS) {
    return false;
  }

  uint8_t pixel[4] = {255, 255, 255, 255};
  ktxTexture_SetImageFromMemory(ktxTexture(ktxTex), 0, 0, 0, pixel, 4);

  ktxVulkanDeviceInfo vdi;
  ktxVulkanDeviceInfo_Construct(&vdi, deviceState.physicalDevice,
                                deviceState.device, deviceState.graphicsQueue,
                                deviceState.commandPool, nullptr);

  ktxVulkanTexture ktxVulkanTex;
  if (ktxTexture_VkUploadEx(ktxTexture(ktxTex), &vdi, &ktxVulkanTex,
                            VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) !=
      KTX_SUCCESS) {
    ktxTexture_Destroy(ktxTexture(ktxTex));
    ktxVulkanDeviceInfo_Destruct(&vdi);
    return false;
  }

  outTexture.image = ktxVulkanTex.image;
  outTexture.imageMemory = ktxVulkanTex.deviceMemory;
  outTexture.width = ktxVulkanTex.width;
  outTexture.height = ktxVulkanTex.height;
  outTexture.mipLevels = ktxVulkanTex.levelCount;

  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = outTexture.image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = ktxVulkanTex.imageFormat;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = outTexture.mipLevels;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  if (vkCreateImageView(deviceState.device, &viewInfo, nullptr,
                        &outTexture.imageView) != VK_SUCCESS) {
    return false;
  }

  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_NEAREST;
  samplerInfo.minFilter = VK_FILTER_NEAREST;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.anisotropyEnable = VK_FALSE;
  samplerInfo.maxAnisotropy = 1.0f;
  samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

  if (vkCreateSampler(deviceState.device, &samplerInfo, nullptr,
                      &outTexture.sampler) != VK_SUCCESS) {
    return false;
  }

  ktxTexture_Destroy(ktxTexture(ktxTex));
  ktxVulkanDeviceInfo_Destruct(&vdi);

  return true;
}

void destroy(DeviceState &deviceState, TextureData &texture) {
  if (texture.sampler != VK_NULL_HANDLE) {
    vkDestroySampler(deviceState.device, texture.sampler, nullptr);
  }
  if (texture.imageView != VK_NULL_HANDLE) {
    vkDestroyImageView(deviceState.device, texture.imageView, nullptr);
  }
  if (texture.image != VK_NULL_HANDLE) {
    vkDestroyImage(deviceState.device, texture.image, nullptr);
  }
  if (texture.imageMemory != VK_NULL_HANDLE) {
    vkFreeMemory(deviceState.device, texture.imageMemory, nullptr);
  }
  texture = TextureData{};
}

} // namespace vke::texture
