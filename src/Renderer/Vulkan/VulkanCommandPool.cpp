#include "Mamancraft/Renderer/Vulkan/VulkanCommandPool.hpp"
#include "Mamancraft/Core/Logger.hpp"

#include <stdexcept>

namespace mc {

VulkanCommandPool::VulkanCommandPool(
    const std::unique_ptr<VulkanDevice> &device, uint32_t queueFamilyIndex,
    vk::CommandPoolCreateFlags flags)
    : m_Device(device) {

  vk::CommandPoolCreateInfo poolInfo;
  poolInfo.flags = flags;
  poolInfo.queueFamilyIndex = queueFamilyIndex;

  try {
    m_CommandPool = m_Device->GetLogicalDevice().createCommandPool(poolInfo);
  } catch (const vk::SystemError &e) {
    MC_CRITICAL("Failed to create Command Pool! Error: {}", e.what());
    throw std::runtime_error("failed to create command pool!");
  }
  MC_INFO("Vulkan Command Pool created successfully.");
}

VulkanCommandPool::~VulkanCommandPool() {
  if (m_CommandPool) {
    m_Device->GetLogicalDevice().destroyCommandPool(m_CommandPool);
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
    const std::unique_ptr<VulkanDevice> &device, vk::CommandPool commandPool,
    bool primary)
    : m_Device(device), m_CommandPool(commandPool) {

  vk::CommandBufferAllocateInfo allocInfo;
  allocInfo.commandPool = m_CommandPool;
  allocInfo.level = primary ? vk::CommandBufferLevel::ePrimary
                            : vk::CommandBufferLevel::eSecondary;
  allocInfo.commandBufferCount = 1;

  try {
    std::vector<vk::CommandBuffer> buffers =
        m_Device->GetLogicalDevice().allocateCommandBuffers(allocInfo);
    m_CommandBuffer = buffers[0];
  } catch (const vk::SystemError &e) {
    MC_CRITICAL("Failed to allocate command buffer! Error: {}", e.what());
    throw std::runtime_error("failed to allocate command buffer!");
  }
}

VulkanCommandBuffer::~VulkanCommandBuffer() {
  if (m_CommandBuffer) {
    m_Device->GetLogicalDevice().freeCommandBuffers(m_CommandPool,
                                                    m_CommandBuffer);
  }
}

void VulkanCommandBuffer::Begin(vk::CommandBufferUsageFlags flags) {
  vk::CommandBufferBeginInfo beginInfo;
  beginInfo.flags = flags;

  try {
    m_CommandBuffer.begin(beginInfo);
  } catch (const vk::SystemError &e) {
    MC_CRITICAL("Failed to begin recording command buffer! Error: {}",
                e.what());
    throw std::runtime_error("failed to begin recording command buffer!");
  }
}

void VulkanCommandBuffer::End() {
  try {
    m_CommandBuffer.end();
  } catch (const vk::SystemError &e) {
    MC_CRITICAL("Failed to record command buffer! Error: {}", e.what());
    throw std::runtime_error("failed to record command buffer!");
  }
}

void VulkanCommandBuffer::Reset() { m_CommandBuffer.reset(); }

} // namespace mc
