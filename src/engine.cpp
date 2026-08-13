#include "engine.hpp"

#include <array>
#include <imgui.h>
#include <stdexcept>

namespace vke {

// --- Internal helpers ---

static void create_hiz_descriptors(EngineState &state);
static void destroy_hiz_descriptors(EngineState &state);
static void run_hiz_compute_pass(EngineState &state, uint32_t imageIndex);

static void allocate_command_buffers(EngineState &state) {
  size_t count = swapchain::image_count(state.swapchain);
  state.commandBuffers.resize(count);

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = state.device.commandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = static_cast<uint32_t>(count);

  if (vkAllocateCommandBuffers(state.device.device, &allocInfo,
                               state.commandBuffers.data()) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate command buffers!");
  }
}

void engine::recreate_swapchain(EngineState &state) {
  int width = 0, height = 0;
  glfwGetFramebufferSize(state.window.handle, &width, &height);
  while (width == 0 || height == 0) {
    glfwGetFramebufferSize(state.window.handle, &width, &height);
    glfwWaitEvents();
  }

  vkDeviceWaitIdle(state.device.device);

  destroy_hiz_descriptors(state);

  swapchain::recreate(state.swapchain, state.device,
                      window::get_extent(state.window));

  create_hiz_descriptors(state);

  if (!state.commandBuffers.empty()) {
    vkFreeCommandBuffers(state.device.device, state.device.commandPool,
                         static_cast<uint32_t>(state.commandBuffers.size()),
                         state.commandBuffers.data());
    state.commandBuffers.clear();
  }
  allocate_command_buffers(state);
}

static void draw_frame(EngineState &state, engine::DrawCallback draw_fn) {
  uint32_t imageIndex;
  auto result = swapchain::acquire_next_image(state.swapchain,
                                              state.device.device, &imageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    engine::recreate_swapchain(state);
    return;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("failed to acquire swap chain image!");
  }

  // Read and reset SSBO stats
  if (state.statsMapped.size() > imageIndex && state.statsMapped[imageIndex] != nullptr) {
      uint32_t* mapped = reinterpret_cast<uint32_t*>(state.statsMapped[imageIndex]);
      state.taskInvocations = mapped[0];
      state.meshInvocations = mapped[1];
      
      // Reset for the new frame
      mapped[0] = 0;
      mapped[1] = 0;
  }

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  if (vkBeginCommandBuffer(state.commandBuffers[imageIndex], &beginInfo) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to begin recording command buffer!");
  }


  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = state.swapchain.renderPass;
  renderPassInfo.framebuffer = state.swapchain.framebuffers[imageIndex];
  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = state.swapchain.extent;

  std::array<VkClearValue, 2> clearValues{};
  clearValues[0].color = {{state.clearColor[0], state.clearColor[1],
                           state.clearColor[2], state.clearColor[3]}};
  clearValues[1].depthStencil = {0.0f, 0};
  renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
  renderPassInfo.pClearValues = clearValues.data();

  vkCmdBeginRenderPass(state.commandBuffers[imageIndex], &renderPassInfo,
                       VK_SUBPASS_CONTENTS_INLINE);

  float winW = static_cast<float>(state.swapchain.extent.width);
  float winH = static_cast<float>(state.swapchain.extent.height);
  float winAspect = winW / winH;
  float targetAspect = static_cast<float>(state.baseWidth) /
                       static_cast<float>(state.baseHeight);

  VkViewport viewport{};
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  VkRect2D scissor{};

  float scaleX = 1.0f;
  float scaleY = 1.0f;

  if (state.aspectMode == AspectMode::FIXED) {
    float vpW = winW;
    float vpH = winH;
    float vpX = 0.0f;
    float vpY = 0.0f;

    if (winAspect > targetAspect) {
      vpW = winH * targetAspect;
      vpX = (winW - vpW) / 2.0f;
    } else {
      vpH = winW / targetAspect;
      vpY = (winH - vpH) / 2.0f;
    }

    viewport.x = vpX;
    viewport.y = vpY;
    viewport.width = vpW;
    viewport.height = vpH;
    scissor.offset = {static_cast<int32_t>(vpX), static_cast<int32_t>(vpY)};
    scissor.extent = {static_cast<uint32_t>(vpW), static_cast<uint32_t>(vpH)};
  } else {
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = winW;
    viewport.height = winH;
    scissor.offset = {0, 0};
    scissor.extent = state.swapchain.extent;

    if (winAspect > targetAspect) {
      scaleX = targetAspect / winAspect;
    } else {
      scaleY = winAspect / targetAspect;
    }
  }

  state.currentImageIndex = imageIndex;

  vkCmdSetViewport(state.commandBuffers[imageIndex], 0, 1, &viewport);
  vkCmdSetScissor(state.commandBuffers[imageIndex], 0, 1, &scissor);

  pipeline::bind(state.pipeline, state.commandBuffers[imageIndex]);
  draw_fn(state, state.commandBuffers[imageIndex]);

  gui::end_frame(state.commandBuffers[imageIndex]);

  vkCmdEndRenderPass(state.commandBuffers[imageIndex]);
  run_hiz_compute_pass(state, imageIndex);

  if (state.triggerFreezeCopy) {
      VkImageCopy copyRegion{};
      copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      copyRegion.srcSubresource.baseArrayLayer = 0;
      copyRegion.srcSubresource.layerCount = 1;
      copyRegion.srcSubresource.mipLevel = 0; // We need to copy ALL mip levels, wait!
      
      // Copy all mip levels
      for (uint32_t mip = 0; mip < state.swapchain.hizMipLevels; mip++) {
          VkImageCopy region{};
          region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
          region.srcSubresource.baseArrayLayer = 0;
          region.srcSubresource.layerCount = 1;
          region.srcSubresource.mipLevel = mip;
          region.dstSubresource = region.srcSubresource;
          
          uint32_t mipWidth = std::max(1u, state.swapchain.extent.width >> (mip + 1));
          uint32_t mipHeight = std::max(1u, state.swapchain.extent.height >> (mip + 1));
          region.extent = {mipWidth, mipHeight, 1};
          
          vkCmdCopyImage(state.commandBuffers[imageIndex],
                         state.swapchain.hizImages[imageIndex], VK_IMAGE_LAYOUT_GENERAL,
                         state.swapchain.frozenHizImage, VK_IMAGE_LAYOUT_GENERAL,
                         1, &region);
      }
      
      state.triggerFreezeCopy = false;
  }

  if (vkEndCommandBuffer(state.commandBuffers[imageIndex]) != VK_SUCCESS) {
    throw std::runtime_error("failed to record command buffer!");
  }

  result = swapchain::submit_and_present(state.swapchain, state.device,
                                         &state.commandBuffers[imageIndex],
                                         &imageIndex);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
      window::was_window_resized(state.window)) {
    window::reset_window_resized_flag(state.window);
    engine::recreate_swapchain(state);
  } else if (result != VK_SUCCESS) {
    throw std::runtime_error("failed to present swap chain image!");
  }
}

static void create_hiz_descriptors(EngineState &state) {
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_NEAREST;
  samplerInfo.minFilter = VK_FILTER_NEAREST;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
  
  if (vkCreateSampler(state.device.device, &samplerInfo, nullptr, &state.depthSampler) != VK_SUCCESS) {
    throw std::runtime_error("failed to create depth sampler!");
  }

  size_t imageCount = state.swapchain.images.size();
  state.hizDescriptorSets.resize(imageCount);
  for (size_t i = 0; i < imageCount; i++) {
    state.hizDescriptorSets[i].resize(state.swapchain.hizMipLevels);
    
    std::vector<VkDescriptorSetLayout> layouts(state.swapchain.hizMipLevels, state.pipeline.hizSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = state.descriptorPool;
    allocInfo.descriptorSetCount = state.swapchain.hizMipLevels;
    allocInfo.pSetLayouts = layouts.data();

    if (vkAllocateDescriptorSets(state.device.device, &allocInfo, state.hizDescriptorSets[i].data()) != VK_SUCCESS) {
      throw std::runtime_error("failed to allocate hiz descriptor sets!");
    }

    for (uint32_t mip = 0; mip < state.swapchain.hizMipLevels; mip++) {
      VkDescriptorImageInfo inputImageInfo{};
      inputImageInfo.sampler = state.depthSampler;
      inputImageInfo.imageLayout = (mip == 0) ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
      // If mip == 0, the input is the actual depth buffer!
      // Otherwise, the input is the previous mip level of the Hi-Z pyramid.
      if (mip == 0) {
        inputImageInfo.imageView = state.swapchain.depthImageView;
      } else {
        inputImageInfo.imageView = state.swapchain.hizMipImageViews[i][mip - 1];
      }

      VkDescriptorImageInfo outputImageInfo{};
      outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
      outputImageInfo.imageView = state.swapchain.hizMipImageViews[i][mip];

      std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

      descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptorWrites[0].dstSet = state.hizDescriptorSets[i][mip];
      descriptorWrites[0].dstBinding = 0;
      descriptorWrites[0].dstArrayElement = 0;
      descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      descriptorWrites[0].descriptorCount = 1;
      descriptorWrites[0].pImageInfo = &inputImageInfo;

      descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptorWrites[1].dstSet = state.hizDescriptorSets[i][mip];
      descriptorWrites[1].dstBinding = 1;
      descriptorWrites[1].dstArrayElement = 0;
      descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
      descriptorWrites[1].descriptorCount = 1;
      descriptorWrites[1].pImageInfo = &outputImageInfo;

      vkUpdateDescriptorSets(state.device.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
  }

  // Allocate frame descriptor sets (Set 2)
  state.frameDescriptorSets.resize(imageCount);
  
  // Create stats SSBOs
  state.statsBuffers.resize(imageCount);
  state.statsBuffersMemory.resize(imageCount);
  state.statsMapped.resize(imageCount);
  
  for (size_t i = 0; i < imageCount; i++) {
      VkDeviceSize bufferSize = sizeof(uint32_t) * 2;
      device::create_buffer(state.device, bufferSize, 
                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                            state.statsBuffers[i], state.statsBuffersMemory[i]);
      vkMapMemory(state.device.device, state.statsBuffersMemory[i], 0, bufferSize, 0, &state.statsMapped[i]);
  }
  std::vector<VkDescriptorSetLayout> frameLayouts(imageCount, state.pipeline.frameSetLayout);
  VkDescriptorSetAllocateInfo frameAllocInfo{};
  frameAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  frameAllocInfo.descriptorPool = state.descriptorPool;
  frameAllocInfo.descriptorSetCount = static_cast<uint32_t>(imageCount);
  frameAllocInfo.pSetLayouts = frameLayouts.data();

  if (vkAllocateDescriptorSets(state.device.device, &frameAllocInfo, state.frameDescriptorSets.data()) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate frame descriptor sets!");
  }

  for (size_t i = 0; i < imageCount; i++) {
    size_t prevFrame = (i + imageCount - 1) % imageCount;

    VkDescriptorImageInfo frameImageInfo{};
    frameImageInfo.sampler = state.depthSampler;
    frameImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    frameImageInfo.imageView = state.swapchain.hizImageViews[prevFrame];

    VkDescriptorImageInfo frozenFrameImageInfo{};
    frozenFrameImageInfo.sampler = state.depthSampler;
    frozenFrameImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    frozenFrameImageInfo.imageView = state.swapchain.frozenHizImageView;

    VkDescriptorBufferInfo statsBufferInfo{};
    statsBufferInfo.buffer = state.statsBuffers[i];
    statsBufferInfo.offset = 0;
    statsBufferInfo.range = sizeof(uint32_t) * 2;

    std::array<VkWriteDescriptorSet, 3> descriptorWrites{};

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = state.frameDescriptorSets[i];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pImageInfo = &frameImageInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = state.frameDescriptorSets[i];
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &frozenFrameImageInfo;

    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].dstSet = state.frameDescriptorSets[i];
    descriptorWrites[2].dstBinding = 2;
    descriptorWrites[2].dstArrayElement = 0;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pBufferInfo = &statsBufferInfo;

    vkUpdateDescriptorSets(state.device.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
  }
}

static void destroy_hiz_descriptors(EngineState &state) {
  for (size_t i = 0; i < state.hizDescriptorSets.size(); i++) {
    if (!state.hizDescriptorSets[i].empty()) {
      vkFreeDescriptorSets(state.device.device, state.descriptorPool,
                           static_cast<uint32_t>(state.hizDescriptorSets[i].size()),
                           state.hizDescriptorSets[i].data());
    }
  }
  state.hizDescriptorSets.clear();

  if (!state.frameDescriptorSets.empty()) {
    vkFreeDescriptorSets(state.device.device, state.descriptorPool,
                         static_cast<uint32_t>(state.frameDescriptorSets.size()),
                         state.frameDescriptorSets.data());
    state.frameDescriptorSets.clear();
  }
  
  for (size_t i = 0; i < state.statsBuffers.size(); i++) {
      vkUnmapMemory(state.device.device, state.statsBuffersMemory[i]);
      vkDestroyBuffer(state.device.device, state.statsBuffers[i], nullptr);
      vkFreeMemory(state.device.device, state.statsBuffersMemory[i], nullptr);
  }
  state.statsBuffers.clear();
  state.statsBuffersMemory.clear();
  state.statsMapped.clear();

  if (state.depthSampler != VK_NULL_HANDLE) {
    vkDestroySampler(state.device.device, state.depthSampler, nullptr);
    state.depthSampler = VK_NULL_HANDLE;
  }
}

static void run_hiz_compute_pass(EngineState &state, uint32_t imageIndex) {
  VkCommandBuffer cmd = state.commandBuffers[imageIndex];

  // Transition Depth Image to SHADER_READ_ONLY_OPTIMAL
  VkImageMemoryBarrier depthBarrier{};
  depthBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  depthBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
  depthBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  depthBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  depthBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  depthBarrier.image = state.swapchain.depthImage;
  depthBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  depthBarrier.subresourceRange.baseMipLevel = 0;
  depthBarrier.subresourceRange.levelCount = 1;
  depthBarrier.subresourceRange.baseArrayLayer = 0;
  depthBarrier.subresourceRange.layerCount = 1;
  depthBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  depthBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &depthBarrier);

  // Transition all mips of Hi-Z Image to GENERAL (from UNDEFINED or GENERAL)
  VkImageMemoryBarrier hizBarrier{};
  hizBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  hizBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; 
  hizBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
  hizBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  hizBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  hizBarrier.image = state.swapchain.hizImages[imageIndex];
  hizBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  hizBarrier.subresourceRange.baseMipLevel = 0;
  hizBarrier.subresourceRange.levelCount = state.swapchain.hizMipLevels;
  hizBarrier.subresourceRange.baseArrayLayer = 0;
  hizBarrier.subresourceRange.layerCount = 1;
  hizBarrier.srcAccessMask = 0;
  hizBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &hizBarrier);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, state.pipeline.hizPipeline);

  uint32_t currentWidth = state.swapchain.extent.width;
  uint32_t currentHeight = state.swapchain.extent.height;

  for (uint32_t mip = 0; mip < state.swapchain.hizMipLevels; mip++) {
    // Each mip level calculates the min depth of a 2x2 area from the previous level.
    // If mip == 0, it reads from the full depth buffer and writes to hiz mip 0.
    // However, hiz mip 0 IS the same size as depth buffer if we just copy it, but wait:
    // If hiz mip 0 is half the size, then currentWidth = max(1, width / 2).
    // Let's assume hiz mip 0 is the full size! No, my compute shader uses invOutputSize.
    // Let's make hiz mip 0 half the size of depth buffer.
    
    currentWidth = std::max(1u, currentWidth / 2);
    currentHeight = std::max(1u, currentHeight / 2);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, state.pipeline.hizLayout, 0, 1,
                            &state.hizDescriptorSets[imageIndex][mip], 0, nullptr);

    struct {
      glm::vec2 invOutputSize;
      int sourceMip;
    } push;
    push.invOutputSize = glm::vec2(1.0f / currentWidth, 1.0f / currentHeight);
    push.sourceMip = 0; // The shader always samples from mip 0 because the view has levelCount=1

    vkCmdPushConstants(cmd, state.pipeline.hizLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);

    uint32_t groupX = (currentWidth + 15) / 16;
    uint32_t groupY = (currentHeight + 15) / 16;
    vkCmdDispatch(cmd, groupX, groupY, 1);

    if (mip < state.swapchain.hizMipLevels - 1) {
      VkImageMemoryBarrier mipBarrier{};
      mipBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      mipBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
      mipBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
      mipBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      mipBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      mipBarrier.image = state.swapchain.hizImages[imageIndex];
      mipBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      mipBarrier.subresourceRange.baseMipLevel = mip;
      mipBarrier.subresourceRange.levelCount = 1;
      mipBarrier.subresourceRange.baseArrayLayer = 0;
      mipBarrier.subresourceRange.layerCount = 1;
      mipBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
      mipBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

      vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0,
                           nullptr, 1, &mipBarrier);
    }
  }
}

// --- Public API ---

namespace engine {

void init(EngineState &state, const EngineConfig &config) {
  state.baseWidth = config.windowWidth;
  state.baseHeight = config.windowHeight;
  for (int i = 0; i < 4; ++i)
    state.clearColor[i] = config.clearColor[i];

  window::create(state.window, config.windowWidth, config.windowHeight,
                 config.windowTitle);
  glfwSetWindowUserPointer(state.window.handle, &state);
  input::init(state.input, state.window);

  device::create(state.device, state.window);
  swapchain::create(state.swapchain, state.device,
                    window::get_extent(state.window));
  pipeline::create(state.pipeline, state.device.device,
                   state.swapchain.renderPass, config.taskShaderPath, config.meshShaderPath,
                   config.fragmentShaderPath);
  allocate_command_buffers(state);

  std::array<VkDescriptorPoolSize, 3> poolSizes{};
  poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSizes[0].descriptorCount = 100000;
  poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  poolSizes[1].descriptorCount = 100;
  poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  poolSizes[2].descriptorCount = 100;

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  poolInfo.maxSets = 50;
  poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

  if (vkCreateDescriptorPool(state.device.device, &poolInfo, nullptr,
                             &state.descriptorPool) != VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor pool!");
  }

  // Allocate Global Descriptor Set
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = state.descriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &state.pipeline.globalSetLayout;

  // We must define how many descriptors we are actually allocating for the unbounded array
  uint32_t maxBinding = 100000;
  VkDescriptorSetVariableDescriptorCountAllocateInfo variableAllocInfo{};
  variableAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
  variableAllocInfo.descriptorSetCount = 1;
  variableAllocInfo.pDescriptorCounts = &maxBinding;
  allocInfo.pNext = &variableAllocInfo;

  if (vkAllocateDescriptorSets(state.device.device, &allocInfo,
                               &state.globalDescriptorSet) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate global descriptor set!");
  }

  create_hiz_descriptors(state);

  gui::init(state.imgui, state);
}

void run(EngineState &state, UpdateCallback update_fn, DrawCallback draw_fn) {
  double last_time = glfwGetTime();

  while (!window::should_close(state.window)) {
    glfwPollEvents();

    double current_time = glfwGetTime();
    float dt = static_cast<float>(current_time - last_time);
    if (dt > 0.1f)
      dt = 0.1f; // Cap dt at 10 FPS to prevent physics explosions
    last_time = current_time;

    gui::begin_frame();
    update_fn(state, dt);

    draw_frame(state, draw_fn);
  }
  vkDeviceWaitIdle(state.device.device);
}

void cleanup(EngineState &state) {
  destroy_hiz_descriptors(state);

  vkDestroyDescriptorPool(state.device.device, state.descriptorPool, nullptr);
  gui::destroy(state.imgui, state.device.device);
  pipeline::destroy(state.pipeline, state.device.device);
  swapchain::destroy(state.swapchain, state.device.device);
  device::destroy(state.device);
  window::destroy(state.window);
}

} // namespace engine
} // namespace vke
