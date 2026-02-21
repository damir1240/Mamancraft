#include "Mamancraft/Renderer/Vulkan/VulkanPipeline.hpp"
#include "Mamancraft/Core/Logger.hpp"

#include <stdexcept>
#include <vector>

namespace mc {

VulkanPipeline::VulkanPipeline(const std::unique_ptr<VulkanDevice> &device,
                               const VulkanShader &vertShader,
                               const VulkanShader &fragShader,
                               const PipelineConfigInfo &configInfo)
    : m_Device(device) {

  if (configInfo.colorAttachmentFormat == vk::Format::eUndefined) {
    MC_CRITICAL("Cannot create graphics pipeline with an undefined color "
                "attachment format!");
    throw std::runtime_error("Cannot create graphics pipeline with an "
                             "undefined color attachment format!");
  }

  CreatePipelineLayout();

  vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
  vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
  vertShaderStageInfo.module = vertShader.GetShaderModule();
  vertShaderStageInfo.pName = "main";

  vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
  fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
  fragShaderStageInfo.module = fragShader.GetShaderModule();
  fragShaderStageInfo.pName = "main";

  vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                      fragShaderStageInfo};

  vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
  vertexInputInfo.vertexBindingDescriptionCount = 0;
  vertexInputInfo.pVertexBindingDescriptions = nullptr;
  vertexInputInfo.vertexAttributeDescriptionCount = 0;
  vertexInputInfo.pVertexAttributeDescriptions = nullptr;

  vk::GraphicsPipelineCreateInfo pipelineInfo;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &configInfo.inputAssemblyInfo;
  pipelineInfo.pViewportState = &configInfo.viewportInfo;
  pipelineInfo.pRasterizationState = &configInfo.rasterizationInfo;
  pipelineInfo.pMultisampleState = &configInfo.multisampleInfo;
  pipelineInfo.pColorBlendState = &configInfo.colorBlendInfo;
  pipelineInfo.pDepthStencilState = &configInfo.depthStencilInfo;
  pipelineInfo.pDynamicState = &configInfo.dynamicStateInfo;

  pipelineInfo.layout = m_PipelineLayout;

  vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo;
  pipelineRenderingCreateInfo.colorAttachmentCount = 1;
  pipelineRenderingCreateInfo.pColorAttachmentFormats =
      &configInfo.colorAttachmentFormat;
  if (configInfo.depthAttachmentFormat != vk::Format::eUndefined) {
    pipelineRenderingCreateInfo.depthAttachmentFormat =
        configInfo.depthAttachmentFormat;
  }
  pipelineInfo.pNext = &pipelineRenderingCreateInfo;

  pipelineInfo.basePipelineHandle = nullptr;

  try {
    auto result = m_Device->GetLogicalDevice().createGraphicsPipeline(
        nullptr, pipelineInfo);
    if (result.result != vk::Result::eSuccess) {
      throw vk::SystemError(result.result);
    }
    m_GraphicsPipeline = result.value;
  } catch (const vk::SystemError &e) {
    MC_CRITICAL("Failed to create Vulkan Graphics Pipeline! Error: {}",
                e.what());
    throw std::runtime_error("failed to create graphics pipeline!");
  }
}

VulkanPipeline::~VulkanPipeline() {
  vk::Device device = m_Device->GetLogicalDevice();
  if (m_GraphicsPipeline) {
    device.destroyPipeline(m_GraphicsPipeline);
    m_GraphicsPipeline = nullptr;
  }
  if (m_PipelineLayout) {
    device.destroyPipelineLayout(m_PipelineLayout);
    m_PipelineLayout = nullptr;
  }
}

void VulkanPipeline::CreatePipelineLayout() {
  vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
  pipelineLayoutInfo.setLayoutCount = 0;
  pipelineLayoutInfo.pSetLayouts = nullptr;
  pipelineLayoutInfo.pushConstantRangeCount = 0;
  pipelineLayoutInfo.pPushConstantRanges = nullptr;

  try {
    m_PipelineLayout =
        m_Device->GetLogicalDevice().createPipelineLayout(pipelineLayoutInfo);
  } catch (const vk::SystemError &e) {
    MC_CRITICAL("Failed to create Vulkan Pipeline Layout! Error: {}", e.what());
    throw std::runtime_error("failed to create pipeline layout!");
  }
}

void VulkanPipeline::DefaultPipelineConfigInfo(PipelineConfigInfo &configInfo) {
  configInfo.inputAssemblyInfo.topology = vk::PrimitiveTopology::eTriangleList;
  configInfo.inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

  configInfo.viewportInfo.viewportCount = 1;
  configInfo.viewportInfo.pViewports = nullptr; // Defined by dynamic state
  configInfo.viewportInfo.scissorCount = 1;
  configInfo.viewportInfo.pScissors = nullptr; // Defined by dynamic state

  configInfo.rasterizationInfo.depthClampEnable = VK_FALSE;
  configInfo.rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
  configInfo.rasterizationInfo.polygonMode = vk::PolygonMode::eFill;
  configInfo.rasterizationInfo.lineWidth = 1.0f;
  configInfo.rasterizationInfo.cullMode = vk::CullModeFlagBits::eNone;
  configInfo.rasterizationInfo.frontFace = vk::FrontFace::eClockwise;
  configInfo.rasterizationInfo.depthBiasEnable = VK_FALSE;
  configInfo.rasterizationInfo.depthBiasConstantFactor = 0.0f;
  configInfo.rasterizationInfo.depthBiasClamp = 0.0f;
  configInfo.rasterizationInfo.depthBiasSlopeFactor = 0.0f;

  configInfo.multisampleInfo.sampleShadingEnable = VK_FALSE;
  configInfo.multisampleInfo.rasterizationSamples = vk::SampleCountFlagBits::e1;
  configInfo.multisampleInfo.minSampleShading = 1.0f;
  configInfo.multisampleInfo.pSampleMask = nullptr;
  configInfo.multisampleInfo.alphaToCoverageEnable = VK_FALSE;
  configInfo.multisampleInfo.alphaToOneEnable = VK_FALSE;

  configInfo.colorBlendAttachment.colorWriteMask =
      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
      vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  configInfo.colorBlendAttachment.blendEnable = VK_FALSE;
  configInfo.colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eOne;
  configInfo.colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eZero;
  configInfo.colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
  configInfo.colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
  configInfo.colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
  configInfo.colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;

  configInfo.colorBlendInfo.logicOpEnable = VK_FALSE;
  configInfo.colorBlendInfo.logicOp = vk::LogicOp::eCopy;
  configInfo.colorBlendInfo.attachmentCount = 1;
  configInfo.colorBlendInfo.pAttachments = &configInfo.colorBlendAttachment;
  configInfo.colorBlendInfo.blendConstants[0] = 0.0f;
  configInfo.colorBlendInfo.blendConstants[1] = 0.0f;
  configInfo.colorBlendInfo.blendConstants[2] = 0.0f;
  configInfo.colorBlendInfo.blendConstants[3] = 0.0f;

  configInfo.depthStencilInfo.depthTestEnable = VK_TRUE;
  configInfo.depthStencilInfo.depthWriteEnable = VK_TRUE;
  configInfo.depthStencilInfo.depthCompareOp = vk::CompareOp::eLess;
  configInfo.depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
  configInfo.depthStencilInfo.minDepthBounds = 0.0f;
  configInfo.depthStencilInfo.maxDepthBounds = 1.0f;
  configInfo.depthStencilInfo.stencilTestEnable = VK_FALSE;
  configInfo.depthStencilInfo.front = vk::StencilOpState{};
  configInfo.depthStencilInfo.back = vk::StencilOpState{};

  configInfo.dynamicStateEnables = {vk::DynamicState::eViewport,
                                    vk::DynamicState::eScissor};
  configInfo.dynamicStateInfo.pDynamicStates =
      configInfo.dynamicStateEnables.data();
  configInfo.dynamicStateInfo.dynamicStateCount =
      static_cast<uint32_t>(configInfo.dynamicStateEnables.size());
  configInfo.dynamicStateInfo.flags = vk::PipelineDynamicStateCreateFlags{};
}

} // namespace mc
