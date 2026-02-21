#pragma once

#include "Mamancraft/Renderer/Vulkan/VulkanDevice.hpp"
#include <memory>
#include <vulkan/vulkan.h>


namespace mc {

class VulkanRenderPass {
public:
  VulkanRenderPass(const std::unique_ptr<VulkanDevice> &device,
                   VkFormat swapChainImageFormat);
  ~VulkanRenderPass();

  VulkanRenderPass(const VulkanRenderPass &) = delete;
  VulkanRenderPass &operator=(const VulkanRenderPass &) = delete;

  VkRenderPass GetRenderPass() const { return m_RenderPass; }

private:
  const std::unique_ptr<VulkanDevice> &m_Device;
  VkRenderPass m_RenderPass = VK_NULL_HANDLE;
};

} // namespace mc
