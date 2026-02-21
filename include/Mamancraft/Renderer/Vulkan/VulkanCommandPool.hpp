#pragma once

#include "Mamancraft/Renderer/Vulkan/VulkanCore.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanDevice.hpp"
#include <memory>
#include <vector>

namespace mc {

class VulkanCommandBuffer;

class VulkanCommandPool {
public:
  VulkanCommandPool(const std::unique_ptr<VulkanDevice> &device,
                    uint32_t queueFamilyIndex,
                    vk::CommandPoolCreateFlags flags = {});
  ~VulkanCommandPool();

  VulkanCommandPool(const VulkanCommandPool &) = delete;
  VulkanCommandPool &operator=(const VulkanCommandPool &) = delete;

  std::unique_ptr<VulkanCommandBuffer>
  AllocateCommandBuffer(bool primary = true);
  std::vector<std::unique_ptr<VulkanCommandBuffer>>
  AllocateCommandBuffers(uint32_t count, bool primary = true);

  vk::CommandPool GetCommandPool() const { return m_CommandPool; }

private:
  const std::unique_ptr<VulkanDevice> &m_Device;
  vk::CommandPool m_CommandPool = nullptr;
};

class VulkanCommandBuffer {
public:
  VulkanCommandBuffer(const std::unique_ptr<VulkanDevice> &device,
                      vk::CommandPool commandPool, bool primary);
  ~VulkanCommandBuffer();

  VulkanCommandBuffer(const VulkanCommandBuffer &) = delete;
  VulkanCommandBuffer &operator=(const VulkanCommandBuffer &) = delete;

  void Begin(vk::CommandBufferUsageFlags flags = {});
  void End();
  void Reset();

  vk::CommandBuffer GetCommandBuffer() const { return m_CommandBuffer; }

private:
  const std::unique_ptr<VulkanDevice> &m_Device;
  vk::CommandPool m_CommandPool;
  vk::CommandBuffer m_CommandBuffer = nullptr;
};

} // namespace mc
