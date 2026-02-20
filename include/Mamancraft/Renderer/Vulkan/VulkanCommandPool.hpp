#pragma once

#include "Mamancraft/Renderer/Vulkan/VulkanDevice.hpp"
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>


namespace mc {

class VulkanCommandBuffer;

class VulkanCommandPool {
public:
  VulkanCommandPool(const std::unique_ptr<VulkanDevice> &device,
                    uint32_t queueFamilyIndex,
                    VkCommandPoolCreateFlags flags = 0);
  ~VulkanCommandPool();

  VulkanCommandPool(const VulkanCommandPool &) = delete;
  VulkanCommandPool &operator=(const VulkanCommandPool &) = delete;

  std::unique_ptr<VulkanCommandBuffer>
  AllocateCommandBuffer(bool primary = true);
  std::vector<std::unique_ptr<VulkanCommandBuffer>>
  AllocateCommandBuffers(uint32_t count, bool primary = true);

  VkCommandPool GetCommandPool() const { return m_CommandPool; }

private:
  const std::unique_ptr<VulkanDevice> &m_Device;
  VkCommandPool m_CommandPool = VK_NULL_HANDLE;
};

class VulkanCommandBuffer {
public:
  VulkanCommandBuffer(const std::unique_ptr<VulkanDevice> &device,
                      VkCommandPool commandPool, bool primary);
  ~VulkanCommandBuffer();

  VulkanCommandBuffer(const VulkanCommandBuffer &) = delete;
  VulkanCommandBuffer &operator=(const VulkanCommandBuffer &) = delete;

  void Begin(VkCommandBufferUsageFlags flags = 0);
  void End();
  void Reset();

  VkCommandBuffer GetCommandBuffer() const { return m_CommandBuffer; }

private:
  const std::unique_ptr<VulkanDevice> &m_Device;
  VkCommandPool m_CommandPool;
  VkCommandBuffer m_CommandBuffer = VK_NULL_HANDLE;
};

} // namespace mc
