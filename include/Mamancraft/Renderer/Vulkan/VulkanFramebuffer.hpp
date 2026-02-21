#pragma once

#include "Mamancraft/Renderer/Vulkan/VulkanDevice.hpp"
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>


namespace mc {

class VulkanFramebuffer {
public:
  VulkanFramebuffer(const std::unique_ptr<VulkanDevice> &device,
                    VkRenderPass renderPass,
                    const std::vector<VkImageView> &attachments,
                    VkExtent2D extent);
  ~VulkanFramebuffer();

  VulkanFramebuffer(const VulkanFramebuffer &) = delete;
  VulkanFramebuffer &operator=(const VulkanFramebuffer &) = delete;

  VkFramebuffer GetFramebuffer() const { return m_Framebuffer; }

private:
  const std::unique_ptr<VulkanDevice> &m_Device;
  VkFramebuffer m_Framebuffer = VK_NULL_HANDLE;
};

} // namespace mc
