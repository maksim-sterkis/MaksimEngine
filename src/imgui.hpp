#pragma once

#include "Device.hpp"
#include "Swapchain.hpp"

namespace vke {

struct ImguiState {
  VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
};

// Forward declare EngineState so we can pass it to gui::init without a circular
// include
struct EngineState;

namespace gui {

void init(ImguiState &state, const EngineState &engine);
void destroy(ImguiState &state, VkDevice device);
void begin_frame();
void end_frame(VkCommandBuffer cmd);

} // namespace gui
} // namespace vke
