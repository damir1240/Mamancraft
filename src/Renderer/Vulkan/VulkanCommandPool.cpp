#include "Mamancraft/Renderer/Vulkan/VulkanCommandPool.hpp"
#include "Mamancraft/Core/Logger.hpp"

#include <stdexcept>

namespace mc {

VulkanCommandPool::VulkanCommandPool(
    const std::unique_ptr<VulkanDevice> &device, uint32_t queueFamilyIndex,
    VkCommandPoolCreateFlags flags)
    : m_Device(device) {

  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = flags;
  poolInfo.queueFamilyIndex = queueFamilyIndex;

  if (vkCreateCommandPool(m_Device->GetLogicalDevice(), &poolInfo, nullptr,
                          &m_CommandPool) != VK_SUCCESS) {
    MC_CRITICAL("Failed to create Command Pool!");
    throw std::runtime_error("failed to create command pool!");
  }
}

VulkanCommandPool::~VulkanCommandPool() {
  if (m_CommandPool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(m_Device->GetLogicalDevice(), m_CommandPool, nullptr);
  }
}

std::unique_ptr<VulkanCommandBuffer>
VulkanCommandPool::AllocateCommandBuffer(bool primary) {
  return std::make_unique<VulkanCommandBuffer>(m_Device, m_CommandPool,
                                               primary);
}

std::vector<std::unique_ptr<VulkanCommandBuffer>>
VulkanCommandPool::AllocateCommandBuffers(uint32_t count, bool primary) {
  std::vector<std::unique_ptr<VulkanCommandBuffer>> buffers;
  buffers.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    buffers.push_back(std::make_unique<VulkanCommandBuffer>(
        m_Device, m_CommandPool, primary));
  }
  return buffers;
}

// -------------------------------------------------------------
// VulkanCommandBuffer Implementation
// -------------------------------------------------------------

VulkanCommandBuffer::VulkanCommandBuffer(
    const std::unique_ptr<VulkanDevice> &device, VkCommandPool commandPool,
    bool primary)
    : m_Device(device), m_CommandPool(commandPool) {

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = m_CommandPool;
  allocInfo.level = primary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY
                            : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
  allocInfo.commandBufferCount = 1;

  if (vkAllocateCommandBuffers(m_Device->GetLogicalDevice(), &allocInfo,
                               &m_CommandBuffer) != VK_SUCCESS) {
    MC_CRITICAL("Failed to allocate command buffer!");
    throw std::runtime_error("failed to allocate command buffer!");
  }
}

VulkanCommandBuffer::~VulkanCommandBuffer() {
  if (m_CommandBuffer != VK_NULL_HANDLE) {
    vkFreeCommandBuffers(m_Device->GetLogicalDevice(), m_CommandPool, 1,
                         &m_CommandBuffer);
  }
}

void VulkanCommandBuffer::Begin(VkCommandBufferUsageFlags flags) {
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = flags;
  beginInfo.pInheritanceInfo = nullptr; // Optional

  if (vkBeginCommandBuffer(m_CommandBuffer, &beginInfo) != VK_SUCCESS) {
    MC_CRITICAL("Failed to begin recording command buffer!");
    throw std::runtime_error("failed to begin recording command buffer!");
  }
}

void VulkanCommandBuffer::End() {
  if (vkEndCommandBuffer(m_CommandBuffer) != VK_SUCCESS) {
    MC_CRITICAL("Failed to record command buffer!");
    throw std::runtime_error("failed to record command buffer!");
  }
}

void VulkanCommandBuffer::Reset() { vkResetCommandBuffer(m_CommandBuffer, 0); }

} // namespace mc
