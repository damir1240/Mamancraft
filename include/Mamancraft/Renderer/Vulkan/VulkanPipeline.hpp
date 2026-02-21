#pragma once

#include "Mamancraft/Renderer/Vulkan/VulkanCore.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanDevice.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanShader.hpp"
#include <memory>
#include <vector>

namespace mc {

struct PipelineConfigInfo {
  PipelineConfigInfo() = default;
  PipelineConfigInfo(const PipelineConfigInfo &) = delete;
  PipelineConfigInfo &operator=(const PipelineConfigInfo &) = delete;

  vk::PipelineViewportStateCreateInfo viewportInfo;
  vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
  vk::PipelineRasterizationStateCreateInfo rasterizationInfo;
  vk::PipelineMultisampleStateCreateInfo multisampleInfo;
  vk::PipelineColorBlendAttachmentState colorBlendAttachment;
  vk::PipelineColorBlendStateCreateInfo colorBlendInfo;
  vk::PipelineDepthStencilStateCreateInfo depthStencilInfo;
  std::vector<vk::DynamicState> dynamicStateEnables;
  vk::PipelineDynamicStateCreateInfo dynamicStateInfo;
  vk::Format colorAttachmentFormat = vk::Format::eUndefined;
  vk::Format depthAttachmentFormat = vk::Format::eUndefined;
  std::vector<vk::VertexInputBindingDescription> bindingDescriptions{};
  std::vector<vk::VertexInputAttributeDescription> attributeDescriptions{};
};

class VulkanPipeline {
public:
  VulkanPipeline(const std::unique_ptr<VulkanDevice> &device,
                 const VulkanShader &vertShader, const VulkanShader &fragShader,
                 const PipelineConfigInfo &configInfo);
  ~VulkanPipeline();

  VulkanPipeline(const VulkanPipeline &) = delete;
  VulkanPipeline &operator=(const VulkanPipeline &) = delete;

  vk::Pipeline GetPipeline() const { return m_GraphicsPipeline; }
  vk::PipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }

  static void DefaultPipelineConfigInfo(PipelineConfigInfo &configInfo);

private:
  void CreatePipelineLayout();

private:
  const std::unique_ptr<VulkanDevice> &m_Device;
  vk::Pipeline m_GraphicsPipeline = nullptr;
  vk::PipelineLayout m_PipelineLayout = nullptr;
};

} // namespace mc
