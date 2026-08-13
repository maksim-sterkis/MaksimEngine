#pragma once

#include "device.hpp"

#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace vke {

struct PushConstantData {
  glm::mat4 mvp;
  glm::mat4 cull_mvp;
  glm::vec4 cull_local_camera_pos;
  glm::vec4 colorOverride;
  int useOverride;
  int useTriplanar;
  int hasTexture;
  int debugColors;
  uint32_t materialIndex;
  uint32_t meshletCount;
  int freezeCulling;
  int padding[5];
};

struct PipelineConfigInfo {
  VkPipelineViewportStateCreateInfo viewportInfo;
  VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
  VkPipelineRasterizationStateCreateInfo rasterizationInfo;
  VkPipelineMultisampleStateCreateInfo multisampleInfo;
  VkPipelineColorBlendAttachmentState colorBlendAttachment;
  VkPipelineColorBlendStateCreateInfo colorBlendInfo;
  VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
  std::vector<VkDynamicState> dynamicStateEnables;
  VkPipelineDynamicStateCreateInfo dynamicStateInfo;
  VkPipelineLayout pipelineLayout = nullptr;
  VkRenderPass renderPass = nullptr;
  uint32_t subpass = 0;
};

struct PipelineState {
  VkPipeline pipeline = VK_NULL_HANDLE;
  VkPipelineLayout layout = VK_NULL_HANDLE;
  VkShaderModule taskModule = VK_NULL_HANDLE;
  VkShaderModule meshModule = VK_NULL_HANDLE;
  VkShaderModule fragModule = VK_NULL_HANDLE;
  VkDescriptorSetLayout modelSetLayout = VK_NULL_HANDLE;
  VkDescriptorSetLayout globalSetLayout = VK_NULL_HANDLE;
  VkDescriptorSetLayout frameSetLayout = VK_NULL_HANDLE;

  VkPipeline hizPipeline = VK_NULL_HANDLE;
  VkPipelineLayout hizLayout = VK_NULL_HANDLE;
  VkShaderModule hizModule = VK_NULL_HANDLE;
  VkDescriptorSetLayout hizSetLayout = VK_NULL_HANDLE;
};

namespace pipeline {

void create(PipelineState &state, VkDevice device, VkRenderPass renderPass,
            const char *taskPath, const char *meshPath, const char *fragPath);
void destroy(PipelineState &state, VkDevice device);
void bind(const PipelineState &state, VkCommandBuffer cmd);
PipelineConfigInfo default_config_info();

} // namespace pipeline
} // namespace vke
