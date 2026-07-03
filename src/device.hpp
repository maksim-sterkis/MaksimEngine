#pragma once

#include "window.hpp"

#include <string>
#include <vector>

namespace vke {

struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
};

struct QueueFamilyIndices {
  uint32_t graphicsFamily = 0;
  uint32_t presentFamily = 0;
  bool graphicsFamilyHasValue = false;
  bool presentFamilyHasValue = false;
  bool isComplete() const {
    return graphicsFamilyHasValue && presentFamilyHasValue;
  }
};

struct DeviceState {
  VkInstance instance = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkQueue graphicsQueue = VK_NULL_HANDLE;
  VkQueue presentQueue = VK_NULL_HANDLE;
  VkCommandPool commandPool = VK_NULL_HANDLE;
  bool supportsFSE = false;

#ifdef NDEBUG
  static constexpr bool enableValidationLayers = false;
#else
  static constexpr bool enableValidationLayers = true;
#endif
};

namespace device {

// Lifecycle
void create(DeviceState &state, WindowState &window);
void destroy(DeviceState &state);

// Queries
SwapChainSupportDetails query_swap_chain_support(const DeviceState &state);
QueueFamilyIndices find_queue_families(const DeviceState &state);
uint32_t find_memory_type(const DeviceState &state, uint32_t typeFilter,
                          VkMemoryPropertyFlags properties);
VkFormat find_supported_format(const DeviceState &state,
                               const std::vector<VkFormat> &candidates,
                               VkImageTiling tiling,
                               VkFormatFeatureFlags features);

// Buffer utilities
void create_buffer(const DeviceState &state, VkDeviceSize size, VkBufferUsageFlags usage,
                   VkMemoryPropertyFlags properties, VkBuffer &buffer,
                   VkDeviceMemory &bufferMemory);
void copy_buffer(const DeviceState &state, VkBuffer srcBuffer, VkBuffer dstBuffer,
                 VkDeviceSize size);

void create_image(const DeviceState &state, uint32_t width, uint32_t height,
                  VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                  VkMemoryPropertyFlags properties, VkImage &image,
                  VkDeviceMemory &imageMemory);

} // namespace device
} // namespace vke
