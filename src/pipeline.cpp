#include "pipeline.hpp"
#include "model.hpp"

#include <fstream>
#include <stdexcept>

namespace vke {

// --- Internal helpers ---

static std::vector<char> read_file(const std::string &filepath) {
  std::ifstream file{filepath, std::ios::ate | std::ios::binary};
  if (!file.is_open()) {
    throw std::runtime_error("failed to open file: " + filepath);
  }
  size_t fileSize = static_cast<size_t>(file.tellg());
  std::vector<char> buffer(fileSize);
  file.seekg(0);
  file.read(buffer.data(), fileSize);
  file.close();
  return buffer;
}

static VkShaderModule create_shader_module(VkDevice device,
                                           const std::vector<char> &code) {
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size();
  createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

  VkShaderModule shaderModule;
  if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create shader module");
  }
  return shaderModule;
}

// --- Public API ---

namespace pipeline {

PipelineConfigInfo default_config_info() {
  PipelineConfigInfo config{};

  config.inputAssemblyInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  config.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  config.inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

  config.viewportInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  config.viewportInfo.viewportCount = 1;
  config.viewportInfo.pViewports = nullptr;
  config.viewportInfo.scissorCount = 1;
  config.viewportInfo.pScissors = nullptr;

  config.rasterizationInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  config.rasterizationInfo.depthClampEnable = VK_FALSE;
  config.rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
  config.rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
  config.rasterizationInfo.lineWidth = 1.0f;
  config.rasterizationInfo.cullMode = VK_CULL_MODE_BACK_BIT;
  config.rasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  config.rasterizationInfo.depthBiasEnable = VK_FALSE;

  config.multisampleInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  config.multisampleInfo.sampleShadingEnable = VK_FALSE;
  config.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  config.colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  config.colorBlendAttachment.blendEnable = VK_FALSE;

  config.colorBlendInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  config.colorBlendInfo.logicOpEnable = VK_FALSE;
  config.colorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
  config.colorBlendInfo.attachmentCount = 1;
  config.colorBlendInfo.pAttachments = &config.colorBlendAttachment;

  config.depthStencilInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  config.depthStencilInfo.depthTestEnable = VK_TRUE;
  config.depthStencilInfo.depthWriteEnable = VK_TRUE;
  config.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
  config.depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
  config.depthStencilInfo.stencilTestEnable = VK_FALSE;

  config.dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT,
                                VK_DYNAMIC_STATE_SCISSOR};
  config.dynamicStateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  config.dynamicStateInfo.pDynamicStates = config.dynamicStateEnables.data();
  config.dynamicStateInfo.dynamicStateCount =
      static_cast<uint32_t>(config.dynamicStateEnables.size());
  config.dynamicStateInfo.flags = 0;

  return config;
}

void create(PipelineState &state, VkDevice device, VkRenderPass renderPass,
            const char *taskPath, const char *meshPath, const char *fragPath) {
  // Create pipeline layout
  VkPushConstantRange pushConstantRange{};
  pushConstantRange.stageFlags =
      VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT;
  pushConstantRange.offset = 0;
  pushConstantRange.size = sizeof(PushConstantData);

  // Descriptor Set 0: Model SSBOs (Meshlets, Vertices, Triangles, Bounds, GlobalVertexBuffer)
  std::vector<VkDescriptorSetLayoutBinding> bindings(5);
  for (int i = 0; i < 5; ++i) {
      bindings[i].binding = i;
      bindings[i].descriptorCount = 1;
      bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      bindings[i].pImmutableSamplers = nullptr;
      bindings[i].stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT;
  }

  VkDescriptorSetLayoutCreateInfo layoutInfoDSL{};
  layoutInfoDSL.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfoDSL.bindingCount = static_cast<uint32_t>(bindings.size());
  layoutInfoDSL.pBindings = bindings.data();

  if (vkCreateDescriptorSetLayout(device, &layoutInfoDSL, nullptr,
                                  &state.modelSetLayout) != VK_SUCCESS) {
    throw std::runtime_error("failed to create model descriptor set layout!");
  }

  // Descriptor Set 1: Global (Materials, Textures)
  std::vector<VkDescriptorSetLayoutBinding> globalBindings(2);
  globalBindings[0].binding = 0;
  globalBindings[0].descriptorCount = 1;
  globalBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  globalBindings[0].pImmutableSamplers = nullptr;
  globalBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  globalBindings[1].binding = 1;
  globalBindings[1].descriptorCount = 100000;
  globalBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  globalBindings[1].pImmutableSamplers = nullptr;
  globalBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutBindingFlagsCreateInfo layoutBindingFlags{};
  layoutBindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
  layoutBindingFlags.bindingCount = 2;
  VkDescriptorBindingFlags flags[2] = {0, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT};
  layoutBindingFlags.pBindingFlags = flags;

  VkDescriptorSetLayoutCreateInfo globalLayoutInfo{};
  globalLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  globalLayoutInfo.pNext = &layoutBindingFlags;
  globalLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
  globalLayoutInfo.bindingCount = static_cast<uint32_t>(globalBindings.size());
  globalLayoutInfo.pBindings = globalBindings.data();

  if (vkCreateDescriptorSetLayout(device, &globalLayoutInfo, nullptr,
                                  &state.globalSetLayout) != VK_SUCCESS) {
    throw std::runtime_error("failed to create global descriptor set layout!");
  }

  VkDescriptorSetLayout layouts[] = {state.modelSetLayout, state.globalSetLayout};

  VkPipelineLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layoutInfo.setLayoutCount = 2;
  layoutInfo.pSetLayouts = layouts;
  layoutInfo.pushConstantRangeCount = 1;
  layoutInfo.pPushConstantRanges = &pushConstantRange;

  if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &state.layout) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create pipeline layout!");
  }

  // Load shaders
  auto taskCode = read_file(taskPath);
  auto meshCode = read_file(meshPath);
  auto fragCode = read_file(fragPath);
  state.taskModule = create_shader_module(device, taskCode);
  state.meshModule = create_shader_module(device, meshCode);
  state.fragModule = create_shader_module(device, fragCode);

  // Shader stages
  VkPipelineShaderStageCreateInfo shaderStages[3];
  shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[0].stage = VK_SHADER_STAGE_TASK_BIT_EXT;
  shaderStages[0].module = state.taskModule;
  shaderStages[0].pName = "main";
  shaderStages[0].flags = 0;
  shaderStages[0].pNext = nullptr;
  shaderStages[0].pSpecializationInfo = nullptr;

  shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[1].stage = VK_SHADER_STAGE_MESH_BIT_EXT;
  shaderStages[1].module = state.meshModule;
  shaderStages[1].pName = "main";
  shaderStages[1].flags = 0;
  shaderStages[1].pNext = nullptr;
  shaderStages[1].pSpecializationInfo = nullptr;

  shaderStages[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[2].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  shaderStages[2].module = state.fragModule;
  shaderStages[2].pName = "main";
  shaderStages[2].flags = 0;
  shaderStages[2].pNext = nullptr;
  shaderStages[2].pSpecializationInfo = nullptr;

  // Vertex input (Disabled for Mesh Shaders)
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexAttributeDescriptionCount = 0;
  vertexInputInfo.vertexBindingDescriptionCount = 0;
  vertexInputInfo.pVertexAttributeDescriptions = nullptr;
  vertexInputInfo.pVertexBindingDescriptions = nullptr;

  // Use default config
  PipelineConfigInfo config = default_config_info();
  config.renderPass = renderPass;
  config.pipelineLayout = state.layout;

  // Create the graphics pipeline
  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 3;
  pipelineInfo.pStages = shaderStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &config.inputAssemblyInfo;
  pipelineInfo.pViewportState = &config.viewportInfo;
  pipelineInfo.pRasterizationState = &config.rasterizationInfo;
  pipelineInfo.pMultisampleState = &config.multisampleInfo;
  pipelineInfo.pColorBlendState = &config.colorBlendInfo;
  pipelineInfo.pDepthStencilState = &config.depthStencilInfo;
  pipelineInfo.pDynamicState = &config.dynamicStateInfo;
  pipelineInfo.layout = state.layout;
  pipelineInfo.renderPass = renderPass;
  pipelineInfo.subpass = 0;
  pipelineInfo.basePipelineIndex = -1;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

  if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                nullptr, &state.pipeline) != VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
  }
}

void destroy(PipelineState &state, VkDevice device) {
  vkDestroyShaderModule(device, state.taskModule, nullptr);
  vkDestroyShaderModule(device, state.meshModule, nullptr);
  vkDestroyShaderModule(device, state.fragModule, nullptr);
  vkDestroyPipeline(device, state.pipeline, nullptr);
  vkDestroyPipelineLayout(device, state.layout, nullptr);
  vkDestroyDescriptorSetLayout(device, state.modelSetLayout, nullptr);
  vkDestroyDescriptorSetLayout(device, state.globalSetLayout, nullptr);
}

void bind(const PipelineState &state, VkCommandBuffer cmd) {
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state.pipeline);
}

} // namespace pipeline
} // namespace vke
