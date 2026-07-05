#include "engine.hpp"

#include <array>
#include <imgui.h>
#include <stdexcept>

namespace vke {

// --- Internal helpers ---

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

  swapchain::recreate(state.swapchain, state.device,
                      window::get_extent(state.window));

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
  clearValues[1].depthStencil = {1.0f, 0};
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

  vkCmdSetViewport(state.commandBuffers[imageIndex], 0, 1, &viewport);
  vkCmdSetScissor(state.commandBuffers[imageIndex], 0, 1, &scissor);

  pipeline::bind(state.pipeline, state.commandBuffers[imageIndex]);
  draw_fn(state, state.commandBuffers[imageIndex]);

  gui::end_frame(state.commandBuffers[imageIndex]);

  vkCmdEndRenderPass(state.commandBuffers[imageIndex]);
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
                   state.swapchain.renderPass, config.vertexShaderPath,
                   config.fragmentShaderPath);
  allocate_command_buffers(state);

  VkDescriptorPoolSize poolSize{};
  poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSize.descriptorCount = 100;

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = 1;
  poolInfo.pPoolSizes = &poolSize;
  poolInfo.maxSets = 100;
  poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

  if (vkCreateDescriptorPool(state.device.device, &poolInfo, nullptr,
                             &state.descriptorPool) != VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor pool!");
  }

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
  vkDestroyDescriptorPool(state.device.device, state.descriptorPool, nullptr);
  gui::destroy(state.imgui, state.device.device);
  pipeline::destroy(state.pipeline, state.device.device);
  swapchain::destroy(state.swapchain, state.device.device);
  device::destroy(state.device);
  window::destroy(state.window);
}

} // namespace engine
} // namespace vke
