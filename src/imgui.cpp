#include "imgui.hpp"
#include "engine.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <array>
#include <stdexcept>

namespace vke::gui {

void init(ImguiState &state, const EngineState &engine) {
  VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = 1000;
  pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
  pool_info.pPoolSizes = pool_sizes;

  if (vkCreateDescriptorPool(engine.device.device, &pool_info, nullptr,
                             &state.descriptorPool) != VK_SUCCESS) {
    throw std::runtime_error("failed to create imgui descriptor pool!");
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForVulkan(engine.window.handle, true);

  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = engine.device.instance;
  init_info.PhysicalDevice = engine.device.physicalDevice;
  init_info.Device = engine.device.device;
  init_info.Queue = engine.device.graphicsQueue;
  init_info.DescriptorPool = state.descriptorPool;
  init_info.MinImageCount = MAX_FRAMES_IN_FLIGHT;
  init_info.ImageCount =
      static_cast<uint32_t>(swapchain::image_count(engine.swapchain));
  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  init_info.RenderPass = engine.swapchain.renderPass;
  init_info.Subpass = 0;

  ImGui_ImplVulkan_Init(&init_info);
}

void destroy(ImguiState &state, VkDevice device) {
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  vkDestroyDescriptorPool(device, state.descriptorPool, nullptr);
}

void begin_frame() {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void end_frame(VkCommandBuffer cmd) {
  ImGui::Render();
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

} // namespace vke::gui
