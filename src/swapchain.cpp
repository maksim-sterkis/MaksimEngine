#include "swapchain.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>


namespace vke {

// --- Internal helpers ---

static VkSurfaceFormatKHR
choose_surface_format(const std::vector<VkSurfaceFormatKHR> &availableFormats) {
  for (const auto &fmt : availableFormats) {
    if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB &&
        fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return fmt;
    }
  }
  return availableFormats[0];
}

static VkPresentModeKHR
choose_present_mode(const std::vector<VkPresentModeKHR> &availableModes,
                    bool vsync) {
  if (vsync)
    return VK_PRESENT_MODE_FIFO_KHR;
  for (const auto &mode : availableModes) {
    if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      return mode;
    }
  }
  return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR &capabilities,
                                VkExtent2D windowExtent) {
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  }
  VkExtent2D actual = windowExtent;
  actual.width =
      std::max(capabilities.minImageExtent.width,
               std::min(capabilities.maxImageExtent.width, actual.width));
  actual.height =
      std::max(capabilities.minImageExtent.height,
               std::min(capabilities.maxImageExtent.height, actual.height));
  return actual;
}

static void create_swapchain_khr(SwapchainState &state, const DeviceState &dev,
                                 VkExtent2D windowExtent,
                                 VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE) {
  SwapChainSupportDetails support = device::query_swap_chain_support(dev);

  VkSurfaceFormatKHR surfaceFormat = choose_surface_format(support.formats);
  VkPresentModeKHR presentMode =
      choose_present_mode(support.presentModes, state.vsync);
  VkExtent2D extent = choose_extent(support.capabilities, windowExtent);

  uint32_t imageCount = support.capabilities.minImageCount + 1;
  if (support.capabilities.maxImageCount > 0 &&
      imageCount > support.capabilities.maxImageCount) {
    imageCount = support.capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = dev.surface;
  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  QueueFamilyIndices indices = device::find_queue_families(dev);
  uint32_t queueFamilyIndices[] = {indices.graphicsFamily,
                                   indices.presentFamily};

  if (indices.graphicsFamily != indices.presentFamily) {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
  } else {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices = nullptr;
  }

  createInfo.preTransform = support.capabilities.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode = presentMode;
  createInfo.clipped = VK_TRUE;
  createInfo.oldSwapchain = oldSwapchain;

#ifdef _WIN32
  VkSurfaceFullScreenExclusiveInfoEXT fseInfo{};
  fseInfo.sType = VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT;
  fseInfo.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_DISALLOWED_EXT;

  if (state.disallowExclusive && dev.supportsFSE) {
    createInfo.pNext = &fseInfo;
  }
#endif

  if (vkCreateSwapchainKHR(dev.device, &createInfo, nullptr,
                           &state.swapchain) != VK_SUCCESS) {
    throw std::runtime_error("failed to create swap chain!");
  }

  vkGetSwapchainImagesKHR(dev.device, state.swapchain, &imageCount, nullptr);
  state.images.resize(imageCount);
  vkGetSwapchainImagesKHR(dev.device, state.swapchain, &imageCount,
                          state.images.data());

  state.imageFormat = surfaceFormat.format;
  state.extent = extent;
}

static VkFormat find_depth_format(const DeviceState &dev) {
  return device::find_supported_format(
      dev,
      {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
       VK_FORMAT_D24_UNORM_S8_UINT},
      VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

static void create_depth_resources(SwapchainState &state,
                                   const DeviceState &dev) {
  state.depthFormat = find_depth_format(dev);

  device::create_image(dev, state.extent.width, state.extent.height,
                       state.depthFormat, VK_IMAGE_TILING_OPTIMAL,
                       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, state.depthImage,
                       state.depthImageMemory);

  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = state.depthImage;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = state.depthFormat;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  if (vkCreateImageView(dev.device, &viewInfo, nullptr,
                        &state.depthImageView) != VK_SUCCESS) {
    throw std::runtime_error("failed to create depth image view!");
  }
}

static void create_image_views(SwapchainState &state, VkDevice device) {
  state.imageViews.resize(state.images.size());
  for (size_t i = 0; i < state.images.size(); i++) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = state.images[i];
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = state.imageFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &state.imageViews[i]) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to create texture image view!");
    }
  }
}

static void create_render_pass(SwapchainState &state, VkDevice device) {
  VkAttachmentDescription colorAttachment = {};
  colorAttachment.format = state.imageFormat;
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorAttachmentRef = {};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentDescription depthAttachment{};
  depthAttachment.format = state.depthFormat;
  depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthAttachment.finalLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthAttachmentRef{};
  depthAttachmentRef.attachment = 1;
  depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;
  subpass.pDepthStencilAttachment = &depthAttachmentRef;

  VkSubpassDependency dependency = {};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstSubpass = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  std::array<VkAttachmentDescription, 2> attachments = {colorAttachment,
                                                        depthAttachment};
  VkRenderPassCreateInfo renderPassInfo = {};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  renderPassInfo.pAttachments = attachments.data();
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies = &dependency;

  if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &state.renderPass) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create render pass!");
  }
}

static void create_framebuffers(SwapchainState &state, VkDevice device) {
  size_t count = state.images.size();
  state.framebuffers.resize(count);
  for (size_t i = 0; i < count; i++) {
    std::array<VkImageView, 2> attachments = {state.imageViews[i],
                                              state.depthImageView};

    VkFramebufferCreateInfo framebufferInfo = {};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = state.renderPass;
    framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    framebufferInfo.pAttachments = attachments.data();
    framebufferInfo.width = state.extent.width;
    framebufferInfo.height = state.extent.height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(device, &framebufferInfo, nullptr,
                            &state.framebuffers[i]) != VK_SUCCESS) {
      throw std::runtime_error("failed to create framebuffer!");
    }
  }
}

static void create_sync_objects(SwapchainState &state, VkDevice device) {
  size_t imgCount = state.images.size();
  state.imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
  state.renderFinishedSemaphores.resize(imgCount);
  state.inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
  state.imagesInFlight.resize(imgCount, VK_NULL_HANDLE);

  VkSemaphoreCreateInfo semaphoreInfo = {};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceInfo = {};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    if (vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                          &state.imageAvailableSemaphores[i]) != VK_SUCCESS ||
        vkCreateFence(device, &fenceInfo, nullptr, &state.inFlightFences[i]) !=
            VK_SUCCESS) {
      throw std::runtime_error(
          "failed to create synchronization objects for a frame!");
    }
  }

  for (size_t i = 0; i < imgCount; i++) {
    if (vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                          &state.renderFinishedSemaphores[i]) != VK_SUCCESS) {
      throw std::runtime_error("failed to create render finished semaphore!");
    }
  }
}

// --- Public API ---

namespace swapchain {

void create(SwapchainState &state, const DeviceState &dev,
            VkExtent2D windowExtent) {
  create_swapchain_khr(state, dev, windowExtent, VK_NULL_HANDLE);
  create_image_views(state, dev.device);
  create_depth_resources(state, dev);
  create_render_pass(state, dev.device);
  create_framebuffers(state, dev.device);
  create_sync_objects(state, dev.device);
}

void recreate(SwapchainState &state, const DeviceState &dev,
              VkExtent2D windowExtent) {
  VkSwapchainKHR oldSwapchain = state.swapchain;

  for (auto fb : state.framebuffers) {
    vkDestroyFramebuffer(dev.device, fb, nullptr);
  }
  state.framebuffers.clear();

  for (auto view : state.imageViews) {
    vkDestroyImageView(dev.device, view, nullptr);
  }
  state.imageViews.clear();

  vkDestroyImageView(dev.device, state.depthImageView, nullptr);
  vkDestroyImage(dev.device, state.depthImage, nullptr);
  vkFreeMemory(dev.device, state.depthImageMemory, nullptr);

  create_swapchain_khr(state, dev, windowExtent, oldSwapchain);

  if (oldSwapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(dev.device, oldSwapchain, nullptr);
  }

  create_image_views(state, dev.device);
  create_depth_resources(state, dev);
  create_framebuffers(state, dev.device);

  state.imagesInFlight.clear();
  state.imagesInFlight.resize(state.images.size(), VK_NULL_HANDLE);
}

void destroy(SwapchainState &state, VkDevice device) {
  vkDestroyImageView(device, state.depthImageView, nullptr);
  vkDestroyImage(device, state.depthImage, nullptr);
  vkFreeMemory(device, state.depthImageMemory, nullptr);

  for (auto view : state.imageViews) {
    vkDestroyImageView(device, view, nullptr);
  }
  state.imageViews.clear();

  if (state.swapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device, state.swapchain, nullptr);
    state.swapchain = VK_NULL_HANDLE;
  }

  for (auto fb : state.framebuffers) {
    vkDestroyFramebuffer(device, fb, nullptr);
  }

  vkDestroyRenderPass(device, state.renderPass, nullptr);

  for (size_t i = 0; i < state.imageAvailableSemaphores.size(); i++) {
    vkDestroySemaphore(device, state.imageAvailableSemaphores[i], nullptr);
    vkDestroyFence(device, state.inFlightFences[i], nullptr);
  }
  for (size_t i = 0; i < state.renderFinishedSemaphores.size(); i++) {
    vkDestroySemaphore(device, state.renderFinishedSemaphores[i], nullptr);
  }
}

size_t image_count(const SwapchainState &state) { return state.images.size(); }

VkResult acquire_next_image(SwapchainState &state, VkDevice device,
                            uint32_t *imageIndex) {
  vkWaitForFences(device, 1, &state.inFlightFences[state.currentFrame], VK_TRUE,
                  std::numeric_limits<uint64_t>::max());

  return vkAcquireNextImageKHR(
      device, state.swapchain, std::numeric_limits<uint64_t>::max(),
      state.imageAvailableSemaphores[state.currentFrame], VK_NULL_HANDLE,
      imageIndex);
}

VkResult submit_and_present(SwapchainState &state, const DeviceState &dev,
                            const VkCommandBuffer *buffer,
                            uint32_t *imageIndex) {
  if (state.imagesInFlight[*imageIndex] != VK_NULL_HANDLE) {
    vkWaitForFences(dev.device, 1, &state.imagesInFlight[*imageIndex], VK_TRUE,
                    UINT64_MAX);
  }
  state.imagesInFlight[*imageIndex] = state.inFlightFences[state.currentFrame];

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  VkSemaphore waitSemaphores[] = {
      state.imageAvailableSemaphores[state.currentFrame]};
  VkPipelineStageFlags waitStages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;

  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = buffer;

  VkSemaphore signalSemaphores[] = {
      state.renderFinishedSemaphores[*imageIndex]};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  vkResetFences(dev.device, 1, &state.inFlightFences[state.currentFrame]);

  if (vkQueueSubmit(dev.graphicsQueue, 1, &submitInfo,
                    state.inFlightFences[state.currentFrame]) != VK_SUCCESS) {
    throw std::runtime_error("failed to submit draw command buffer!");
  }

  VkPresentInfoKHR presentInfo = {};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;

  VkSwapchainKHR swapChains[] = {state.swapchain};
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapChains;
  presentInfo.pImageIndices = imageIndex;

  auto result = vkQueuePresentKHR(dev.presentQueue, &presentInfo);

  state.currentFrame = (state.currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
  return result;
}

} // namespace swapchain
} // namespace vke
