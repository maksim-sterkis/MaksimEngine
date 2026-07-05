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
  const char *vertexShaderPath = "shaders/shader.vert.spv";
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
  std::vector<VkCommandBuffer> commandBuffers;

  AspectMode aspectMode = AspectMode::ULTRAWIDE;
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
