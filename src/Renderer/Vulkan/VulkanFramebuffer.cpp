#include "Mamancraft/Renderer/Vulkan/VulkanFramebuffer.hpp"
#include "Mamancraft/Core/Logger.hpp"
#include <stdexcept>

namespace mc {

VulkanFramebuffer::VulkanFramebuffer(
    const std::unique_ptr<VulkanDevice> &device, VkRenderPass renderPass,
    const std::vector<VkImageView> &attachments, VkExtent2D extent)
    : m_Device(device) {

  VkFramebufferCreateInfo framebufferInfo{};
  framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferInfo.renderPass = renderPass;
  framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  framebufferInfo.pAttachments = attachments.data();
  framebufferInfo.width = extent.width;
  framebufferInfo.height = extent.height;
  framebufferInfo.layers = 1;

  if (vkCreateFramebuffer(m_Device->GetLogicalDevice(), &framebufferInfo,
                          nullptr, &m_Framebuffer) != VK_SUCCESS) {
    MC_CRITICAL("Failed to create Framebuffer!");
    throw std::runtime_error("failed to create framebuffer!");
  }
  MC_TRACE("Vulkan Framebuffer created successfully.");
}

VulkanFramebuffer::~VulkanFramebuffer() {
  if (m_Framebuffer != VK_NULL_HANDLE) {
    vkDestroyFramebuffer(m_Device->GetLogicalDevice(), m_Framebuffer, nullptr);
  }
}

} // namespace mc
