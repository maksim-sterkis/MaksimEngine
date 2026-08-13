#pragma once

#include "device.hpp"

#include <vector>

namespace vke {

static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

struct SwapchainState {
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  VkFormat imageFormat;
  VkExtent2D extent;
  VkRenderPass renderPass = VK_NULL_HANDLE;

  std::vector<VkImage> images;
  std::vector<VkImageView> imageViews;
  std::vector<VkFramebuffer> framebuffers;

  std::vector<VkSemaphore> imageAvailableSemaphores;
  std::vector<VkSemaphore> renderFinishedSemaphores;
  std::vector<VkFence> inFlightFences;
  std::vector<VkFence> imagesInFlight;
  size_t currentFrame = 0;

  VkImage depthImage = VK_NULL_HANDLE;
  VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
  VkImageView depthImageView = VK_NULL_HANDLE;
  VkFormat depthFormat;

  std::vector<VkImage> hizImages;
  std::vector<VkDeviceMemory> hizImageMemories;
  std::vector<VkImageView> hizImageViews;
  std::vector<std::vector<VkImageView>> hizMipImageViews;
  uint32_t hizMipLevels = 1;

  VkImage frozenHizImage = VK_NULL_HANDLE;
  VkDeviceMemory frozenHizImageMemory = VK_NULL_HANDLE;
  VkImageView frozenHizImageView = VK_NULL_HANDLE;


  bool vsync = false;
  bool disallowExclusive = false;
};

namespace swapchain {

void create(SwapchainState &state, const DeviceState &device,
            VkExtent2D windowExtent);
void recreate(SwapchainState &state, const DeviceState &device,
              VkExtent2D windowExtent);
void destroy(SwapchainState &state, VkDevice device);

size_t image_count(const SwapchainState &state);
VkResult acquire_next_image(SwapchainState &state, VkDevice device,
                            uint32_t *imageIndex);
VkResult submit_and_present(SwapchainState &state, const DeviceState &device,
                            const VkCommandBuffer *buffer,
                            uint32_t *imageIndex);

} // namespace swapchain
} // namespace vke
