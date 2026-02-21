#pragma once

#include "Mamancraft/Renderer/Vulkan/VulkanDevice.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanShader.hpp"
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

namespace mc {

struct PipelineConfigInfo {
  PipelineConfigInfo() = default;
  PipelineConfigInfo(const PipelineConfigInfo &) = delete;
  PipelineConfigInfo &operator=(const PipelineConfigInfo &) = delete;

  VkPipelineViewportStateCreateInfo viewportInfo{};
  VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
  VkPipelineRasterizationStateCreateInfo rasterizationInfo{};
  VkPipelineMultisampleStateCreateInfo multisampleInfo{};
  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  VkPipelineColorBlendStateCreateInfo colorBlendInfo{};
  VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
  std::vector<VkDynamicState> dynamicStateEnables;
  VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
  VkRenderPass renderPass = VK_NULL_HANDLE;
  uint32_t subpass = 0;
};

class VulkanPipeline {
public:
  VulkanPipeline(const std::unique_ptr<VulkanDevice> &device,
                 const VulkanShader &vertShader, const VulkanShader &fragShader,
                 const PipelineConfigInfo &configInfo);
  ~VulkanPipeline();

  VulkanPipeline(const VulkanPipeline &) = delete;
  VulkanPipeline &operator=(const VulkanPipeline &) = delete;

  VkPipeline GetPipeline() const { return m_GraphicsPipeline; }
  VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }

  static void DefaultPipelineConfigInfo(PipelineConfigInfo &configInfo);

private:
  void CreatePipelineLayout();

private:
  const std::unique_ptr<VulkanDevice> &m_Device;
  VkPipeline m_GraphicsPipeline = VK_NULL_HANDLE;
  VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
};

} // namespace mc
