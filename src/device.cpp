#include "device.hpp"

#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>

namespace vke {

// --- Internal constants ---
static const std::vector<const char *> g_validationLayers = {
    "VK_LAYER_KHRONOS_validation"};
static const std::vector<const char *> g_deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

// --- Internal helpers (file-scoped) ---

static VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
               VkDebugUtilsMessageTypeFlagsEXT messageType,
               const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
               void *pUserData) {
  std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
  return VK_FALSE;
}

static VkResult
create_debug_messenger(VkInstance instance,
                       const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                       const VkAllocationCallbacks *pAllocator,
                       VkDebugUtilsMessengerEXT *pDebugMessenger) {
  auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkCreateDebugUtilsMessengerEXT");
  if (func != nullptr) {
    return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
  }
  return VK_ERROR_EXTENSION_NOT_PRESENT;
}

static void destroy_debug_messenger(VkInstance instance,
                                    VkDebugUtilsMessengerEXT debugMessenger,
                                    const VkAllocationCallbacks *pAllocator) {
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != nullptr) {
    func(instance, debugMessenger, pAllocator);
  }
}

static void
populate_debug_create_info(VkDebugUtilsMessengerCreateInfoEXT &createInfo) {
  createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  createInfo.pfnUserCallback = debug_callback;
}

static bool check_validation_layer_support() {
  uint32_t layerCount;
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
  std::vector<VkLayerProperties> availableLayers(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

  for (const char *layerName : g_validationLayers) {
    bool found = false;
    for (const auto &props : availableLayers) {
      if (strcmp(layerName, props.layerName) == 0) {
        found = true;
        break;
      }
    }
    if (!found)
      return false;
  }
  return true;
}

static std::vector<const char *> get_required_extensions(bool validation) {
  uint32_t glfwExtensionCount = 0;
  const char **glfwExtensions =
      glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  std::vector<const char *> extensions(glfwExtensions,
                                       glfwExtensions + glfwExtensionCount);
  if (validation) {
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }
#ifdef _WIN32
  extensions.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
  extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#endif
  return extensions;
}

static void check_glfw_required_extensions(bool validation) {
  uint32_t extensionCount = 0;
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
  std::vector<VkExtensionProperties> extensions(extensionCount);
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount,
                                         extensions.data());

  auto required = get_required_extensions(validation);
  for (const auto &req : required) {
    bool found = false;
    for (const auto &ext : extensions) {
      if (strcmp(req, ext.extensionName) == 0) {
        found = true;
        break;
      }
    }
    if (!found) {
      throw std::runtime_error("Missing required glfw extension");
    }
  }
}

static void create_instance(DeviceState &state) {
  if (state.enableValidationLayers && !check_validation_layer_support()) {
    throw std::runtime_error("validation layers requested, but not available!");
  }

  VkApplicationInfo appInfo = {};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "VK_game_engine";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "No Engine";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_2;

  VkInstanceCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;

  auto extensions = get_required_extensions(state.enableValidationLayers);
  createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  createInfo.ppEnabledExtensionNames = extensions.data();

  VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
  if (state.enableValidationLayers) {
    createInfo.enabledLayerCount =
        static_cast<uint32_t>(g_validationLayers.size());
    createInfo.ppEnabledLayerNames = g_validationLayers.data();
    populate_debug_create_info(debugCreateInfo);
    createInfo.pNext = &debugCreateInfo;
  } else {
    createInfo.enabledLayerCount = 0;
    createInfo.pNext = nullptr;
  }

  if (vkCreateInstance(&createInfo, nullptr, &state.instance) != VK_SUCCESS) {
    throw std::runtime_error("failed to create instance!");
  }

  check_glfw_required_extensions(state.enableValidationLayers);
}

static void setup_debug_messenger(DeviceState &state) {
  if (!state.enableValidationLayers)
    return;
  VkDebugUtilsMessengerCreateInfoEXT createInfo;
  populate_debug_create_info(createInfo);
  if (create_debug_messenger(state.instance, &createInfo, nullptr,
                             &state.debugMessenger) != VK_SUCCESS) {
    throw std::runtime_error("failed to set up debug messenger!");
  }
}

static QueueFamilyIndices find_queue_families_for(VkPhysicalDevice device,
                                                  VkSurfaceKHR surface) {
  QueueFamilyIndices indices;
  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                           queueFamilies.data());

  int i = 0;
  for (const auto &queueFamily : queueFamilies) {
    if (queueFamily.queueCount > 0 &&
        queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.graphicsFamily = i;
      indices.graphicsFamilyHasValue = true;
    }
    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
    if (queueFamily.queueCount > 0 && presentSupport) {
      indices.presentFamily = i;
      indices.presentFamilyHasValue = true;
    }
    if (indices.isComplete())
      break;
    i++;
  }
  return indices;
}

static SwapChainSupportDetails query_swap_support_for(VkPhysicalDevice device,
                                                      VkSurfaceKHR surface) {
  SwapChainSupportDetails details;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface,
                                            &details.capabilities);

  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
  if (formatCount != 0) {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount,
                                         details.formats.data());
  }

  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount,
                                            nullptr);
  if (presentModeCount != 0) {
    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device, surface, &presentModeCount, details.presentModes.data());
  }
  return details;
}

static bool check_device_extension_support(VkPhysicalDevice device) {
  uint32_t extensionCount;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                       nullptr);
  std::vector<VkExtensionProperties> available(extensionCount);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                       available.data());

  std::set<std::string> required(g_deviceExtensions.begin(),
                                 g_deviceExtensions.end());
  for (const auto &ext : available) {
    required.erase(ext.extensionName);
  }
  return required.empty();
}

static bool is_device_suitable(VkPhysicalDevice device, VkSurfaceKHR surface) {
  QueueFamilyIndices indices = find_queue_families_for(device, surface);
  bool extensionsSupported = check_device_extension_support(device);
  bool swapChainAdequate = false;
  if (extensionsSupported) {
    SwapChainSupportDetails support = query_swap_support_for(device, surface);
    swapChainAdequate =
        !support.formats.empty() && !support.presentModes.empty();
  }
  return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

static void pick_physical_device(DeviceState &state) {
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(state.instance, &deviceCount, nullptr);
  if (deviceCount == 0) {
    throw std::runtime_error("failed to find GPUs with Vulkan support!");
  }
  std::cout << "Device count: " << deviceCount << std::endl;
  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(state.instance, &deviceCount, devices.data());

  for (const auto &dev : devices) {
    if (is_device_suitable(dev, state.surface)) {
      state.physicalDevice = dev;
      break;
    }
  }

  if (state.physicalDevice == VK_NULL_HANDLE) {
    throw std::runtime_error("failed to find a suitable GPU!");
  }

  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(state.physicalDevice, &properties);
  std::cout << "Physical device: " << properties.deviceName << std::endl;
}

static void create_logical_device(DeviceState &state) {
  QueueFamilyIndices indices =
      find_queue_families_for(state.physicalDevice, state.surface);

  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily,
                                            indices.presentFamily};

  float queuePriority = 1.0f;
  for (uint32_t queueFamily : uniqueQueueFamilies) {
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(queueCreateInfo);
  }

  // Build device extensions list (required + optional)
  std::vector<const char *> enabledExtensions(g_deviceExtensions.begin(),
                                              g_deviceExtensions.end());

#ifdef _WIN32
  uint32_t extCount;
  vkEnumerateDeviceExtensionProperties(state.physicalDevice, nullptr, &extCount,
                                       nullptr);
  std::vector<VkExtensionProperties> availableExts(extCount);
  vkEnumerateDeviceExtensionProperties(state.physicalDevice, nullptr, &extCount,
                                       availableExts.data());
  for (const auto &ext : availableExts) {
    if (strcmp(ext.extensionName,
               VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME) == 0) {
      enabledExtensions.push_back(VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME);
      state.supportsFSE = true;
      break;
    }
  }
#endif

  VkPhysicalDeviceVulkan12Features vulkan12Features = {};
  vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  vulkan12Features.descriptorBindingPartiallyBound = VK_TRUE;
  vulkan12Features.runtimeDescriptorArray = VK_TRUE;
  vulkan12Features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
  vulkan12Features.descriptorBindingVariableDescriptorCount = VK_TRUE;
  vulkan12Features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;

  VkPhysicalDeviceFeatures2 physicalDeviceFeatures2 = {};
  physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  physicalDeviceFeatures2.pNext = &vulkan12Features;
  // Note: we can query supported features here with vkGetPhysicalDeviceFeatures2, but we assume RTX 3090 supports it.

  VkDeviceCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.pNext = &physicalDeviceFeatures2;
  createInfo.queueCreateInfoCount =
      static_cast<uint32_t>(queueCreateInfos.size());
  createInfo.pQueueCreateInfos = queueCreateInfos.data();
  createInfo.pEnabledFeatures = nullptr; // Ignored when using pNext chain
  createInfo.enabledExtensionCount =
      static_cast<uint32_t>(enabledExtensions.size());
  createInfo.ppEnabledExtensionNames = enabledExtensions.data();
  createInfo.enabledLayerCount = 0;

  if (vkCreateDevice(state.physicalDevice, &createInfo, nullptr,
                     &state.device) != VK_SUCCESS) {
    throw std::runtime_error("failed to create logical device!");
  }

  vkGetDeviceQueue(state.device, indices.graphicsFamily, 0,
                   &state.graphicsQueue);
  vkGetDeviceQueue(state.device, indices.presentFamily, 0, &state.presentQueue);
}

static void create_command_pool(DeviceState &state) {
  QueueFamilyIndices indices =
      find_queue_families_for(state.physicalDevice, state.surface);

  VkCommandPoolCreateInfo poolInfo = {};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.queueFamilyIndex = indices.graphicsFamily;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                   VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  if (vkCreateCommandPool(state.device, &poolInfo, nullptr,
                          &state.commandPool) != VK_SUCCESS) {
    throw std::runtime_error("failed to create command pool!");
  }
}

// --- Public API ---

namespace device {

void create(DeviceState &state, WindowState &win) {
  create_instance(state);
  setup_debug_messenger(state);
  window::create_surface(win, state.instance, &state.surface);
  pick_physical_device(state);
  create_logical_device(state);
  create_command_pool(state);
}

void destroy(DeviceState &state) {
  vkDestroyCommandPool(state.device, state.commandPool, nullptr);
  vkDestroyDevice(state.device, nullptr);

  if (state.enableValidationLayers) {
    destroy_debug_messenger(state.instance, state.debugMessenger, nullptr);
  }

  vkDestroySurfaceKHR(state.instance, state.surface, nullptr);
  vkDestroyInstance(state.instance, nullptr);
}

SwapChainSupportDetails query_swap_chain_support(const DeviceState &state) {
  return query_swap_support_for(state.physicalDevice, state.surface);
}

QueueFamilyIndices find_queue_families(const DeviceState &state) {
  return find_queue_families_for(state.physicalDevice, state.surface);
}

uint32_t find_memory_type(const DeviceState &state, uint32_t typeFilter,
                          VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(state.physicalDevice, &memProperties);
  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags &
                                    properties) == properties) {
      return i;
    }
  }
  throw std::runtime_error("failed to find suitable memory type!");
}

VkFormat find_supported_format(const DeviceState &state,
                               const std::vector<VkFormat> &candidates,
                               VkImageTiling tiling,
                               VkFormatFeatureFlags features) {
  for (VkFormat format : candidates) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(state.physicalDevice, format, &props);
    if (tiling == VK_IMAGE_TILING_LINEAR &&
        (props.linearTilingFeatures & features) == features) {
      return format;
    } else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
               (props.optimalTilingFeatures & features) == features) {
      return format;
    }
  }
  throw std::runtime_error("failed to find supported format!");
}

void create_buffer(const DeviceState &state, VkDeviceSize size,
                   VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                   VkBuffer &buffer, VkDeviceMemory &bufferMemory) {
  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateBuffer(state.device, &bufferInfo, nullptr, &buffer) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create buffer!");
  }

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(state.device, buffer, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex =
      find_memory_type(state, memRequirements.memoryTypeBits, properties);

  if (vkAllocateMemory(state.device, &allocInfo, nullptr, &bufferMemory) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to allocate buffer memory!");
  }

  vkBindBufferMemory(state.device, buffer, bufferMemory, 0);
}

void copy_buffer(const DeviceState &state, VkBuffer srcBuffer,
                 VkBuffer dstBuffer, VkDeviceSize size) {
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

  VkBufferCopy copyRegion{};
  copyRegion.size = size;
  vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(state.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(state.graphicsQueue);

  vkFreeCommandBuffers(state.device, state.commandPool, 1, &commandBuffer);
}

void create_image(const DeviceState &state, uint32_t width, uint32_t height,
                  VkFormat format, VkImageTiling tiling,
                  VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                  VkImage &image, VkDeviceMemory &imageMemory) {
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = width;
  imageInfo.extent.height = height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format = format;
  imageInfo.tiling = tiling;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = usage;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateImage(state.device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
    throw std::runtime_error("failed to create image!");
  }

  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(state.device, image, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex =
      find_memory_type(state, memRequirements.memoryTypeBits, properties);

  if (vkAllocateMemory(state.device, &allocInfo, nullptr, &imageMemory) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to allocate image memory!");
  }

  vkBindImageMemory(state.device, image, imageMemory, 0);
}

} // namespace device
} // namespace vke
