#pragma once

#include "device.hpp"

#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace vke {

struct PushConstantData {
  glm::mat4 mvp;
  glm::vec4 colorOverride;
  int useOverride;
  int useTriplanar;
  int hasTexture;
  int debugColors;
  uint32_t materialIndex;
  int padding[7];
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
  VkShaderModule vertModule = VK_NULL_HANDLE;
  VkShaderModule fragModule = VK_NULL_HANDLE;
  VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
};

namespace pipeline {

void create(PipelineState &state, VkDevice device, VkRenderPass renderPass,
            const char *vertPath, const char *fragPath);
void destroy(PipelineState &state, VkDevice device);
void bind(const PipelineState &state, VkCommandBuffer cmd);
PipelineConfigInfo default_config_info();

} // namespace pipeline
} // namespace vke
