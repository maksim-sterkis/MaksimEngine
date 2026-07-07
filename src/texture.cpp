#include "texture.hpp"
#include <iostream>
#include <ktx.h>
#include <ktxvulkan.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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

bool load_ktx2_from_memory(DeviceState &deviceState, const uint8_t* data, size_t size,
                           TextureData &outTexture) {
  ktxTexture2 *kTexture;
  KTX_error_code result = ktxTexture2_CreateFromMemory(
      data, size, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &kTexture);
  if (result != KTX_SUCCESS) {
    std::cerr << "Failed to load KTX2 texture from memory! Error: " 
              << ktxErrorString(result) << std::endl;
    return false;
  }

  if (ktxTexture2_NeedsTranscoding(kTexture)) {
    result = ktxTexture2_TranscodeBasis(kTexture, KTX_TTF_BC7_RGBA, 0);
    if (result != KTX_SUCCESS) {
      std::cerr << "Failed to transcode KTX2 basis texture from memory!" << std::endl;
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
    std::cerr << "Failed to upload KTX2 texture from memory!" << std::endl;
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

VkCommandBuffer begin_single_time_commands(const DeviceState& state) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = state.commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(state.device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

void end_single_time_commands(const DeviceState& state, VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(state.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(state.graphicsQueue);

    vkFreeCommandBuffers(state.device, state.commandPool, 1, &commandBuffer);
}

void transition_image_layout(const DeviceState& state, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer commandBuffer = begin_single_time_commands(state);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    end_single_time_commands(state, commandBuffer);
}

void copy_buffer_to_image(const DeviceState& state, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = begin_single_time_commands(state);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    end_single_time_commands(state, commandBuffer);
}

bool load_image_from_memory(DeviceState &deviceState, const uint8_t* data, size_t size, TextureData &outTexture) {
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load_from_memory(data, static_cast<int>(size), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if (!pixels) {
        std::cerr << "failed to load texture image from memory!" << std::endl;
        return false;
    }

    VkDeviceSize imageSize = texWidth * texHeight * 4;
    outTexture.width = texWidth;
    outTexture.height = texHeight;
    outTexture.mipLevels = 1;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    device::create_buffer(deviceState, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* mappedData;
    vkMapMemory(deviceState.device, stagingBufferMemory, 0, imageSize, 0, &mappedData);
    memcpy(mappedData, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(deviceState.device, stagingBufferMemory);

    stbi_image_free(pixels);

    device::create_image(deviceState, texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, outTexture.image, outTexture.imageMemory);

    transition_image_layout(deviceState, outTexture.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copy_buffer_to_image(deviceState, stagingBuffer, outTexture.image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    transition_image_layout(deviceState, outTexture.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(deviceState.device, stagingBuffer, nullptr);
    vkFreeMemory(deviceState.device, stagingBufferMemory, nullptr);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = outTexture.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(deviceState.device, &viewInfo, nullptr, &outTexture.imageView) != VK_SUCCESS) {
        return false;
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
    samplerInfo.maxLod = 1.0f;

    if (vkCreateSampler(deviceState.device, &samplerInfo, nullptr, &outTexture.sampler) != VK_SUCCESS) {
        return false;
    }

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
