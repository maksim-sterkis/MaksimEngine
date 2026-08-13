#pragma once

#include "imgui.hpp"
#include "input.hpp"
#include "pipeline.hpp"
#include "swapchain.hpp"
#include "window.hpp"

#include <vector>

namespace vke {

enum class AspectMode { ULTRAWIDE, FIXED };

struct EngineConfig {
  const char *windowTitle = "Vulkan Engine";
  int windowWidth = 800;
  int windowHeight = 600;
  const char *taskShaderPath = "shaders/shader.task.spv";
  const char *meshShaderPath = "shaders/shader.mesh.spv";
  const char *fragmentShaderPath = "shaders/shader.frag.spv";
  float clearColor[4] = {0.01f, 0.01f, 0.01f, 1.0f};
};

struct EngineState {
  int baseWidth = 800;
  int baseHeight = 600;
  float clearColor[4] = {0.01f, 0.01f, 0.01f, 1.0f};

  WindowState window;
  InputState input;
  DeviceState device;
  SwapchainState swapchain;
  PipelineState pipeline;
  ImguiState imgui;
  VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
  VkDescriptorSet globalDescriptorSet = VK_NULL_HANDLE;
  VkSampler depthSampler = VK_NULL_HANDLE;
  std::vector<std::vector<VkDescriptorSet>> hizDescriptorSets; // [frameInFlight][mipLevel]
  std::vector<VkDescriptorSet> frameDescriptorSets; // [frameInFlight]
  std::vector<VkCommandBuffer> commandBuffers;
  uint32_t currentImageIndex = 0;
  bool triggerFreezeCopy = false;

  AspectMode aspectMode = AspectMode::ULTRAWIDE;
  
  std::vector<VkBuffer> statsBuffers;
  std::vector<VkDeviceMemory> statsBuffersMemory;
  std::vector<void*> statsMapped;
  
  uint64_t taskInvocations = 0;
  uint64_t meshInvocations = 0;
};

namespace engine {

using UpdateCallback = void (*)(EngineState &, float);
using DrawCallback = void (*)(EngineState &, VkCommandBuffer);

void init(EngineState &state, const EngineConfig &config);
void run(EngineState &state, UpdateCallback update_fn, DrawCallback draw_fn);
void recreate_swapchain(EngineState &state);
void cleanup(EngineState &state);

} // namespace engine
} // namespace vke
