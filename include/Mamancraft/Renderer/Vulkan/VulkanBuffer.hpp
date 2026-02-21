#pragma once

#include "Mamancraft/Renderer/Vulkan/VulkanCore.hpp"
#include "Mamancraft/Renderer/VulkanContext.hpp"

namespace mc {

class VulkanBuffer {
public:
  VulkanBuffer(VmaAllocator allocator, vk::DeviceSize instanceSize,
               uint32_t instanceCount, vk::BufferUsageFlags usageFlags,
               VmaMemoryUsage memoryUsage,
               vk::DeviceSize minOffsetAlignment = 1);
  ~VulkanBuffer();

  VulkanBuffer(const VulkanBuffer &) = delete;
  VulkanBuffer &operator=(const VulkanBuffer &) = delete;
  VulkanBuffer(VulkanBuffer &&) = delete;
  VulkanBuffer &operator=(VulkanBuffer &&) = delete;

  vk::Result Map();
  void Unmap();
  void WriteToBuffer(void *data, vk::DeviceSize size = VK_WHOLE_SIZE,
                     vk::DeviceSize offset = 0);
  vk::Result Flush(vk::DeviceSize size = VK_WHOLE_SIZE,
                   vk::DeviceSize offset = 0);
  vk::Result Invalidate(vk::DeviceSize size = VK_WHOLE_SIZE,
                        vk::DeviceSize offset = 0);

  vk::Buffer GetBuffer() const { return m_Buffer; }
  void *GetMappedMemory() const { return m_MappedRegion; }
  uint32_t GetInstanceCount() const { return m_InstanceCount; }
  vk::DeviceSize GetInstanceSize() const { return m_InstanceSize; }
  vk::DeviceSize GetAlignmentSize() const { return m_AlignmentSize; }
  vk::BufferUsageFlags GetUsageFlags() const { return m_UsageFlags; }
  vk::DeviceSize GetBufferSize() const { return m_BufferSize; }

  static void CopyBuffer(VulkanContext &context, vk::Buffer srcBuffer,
                         vk::Buffer dstBuffer, vk::DeviceSize size);

private:
  static vk::DeviceSize GetAlignment(vk::DeviceSize instanceSize,
                                     vk::DeviceSize minOffsetAlignment);

private:
  VmaAllocator m_Allocator;

  void *m_MappedRegion = nullptr;
  vk::Buffer m_Buffer = nullptr;
  VmaAllocation m_Allocation = nullptr;
  VmaAllocationInfo m_AllocationInfo{};

  vk::DeviceSize m_BufferSize;
  uint32_t m_InstanceCount;
  vk::DeviceSize m_InstanceSize;
  vk::DeviceSize m_AlignmentSize;
  vk::BufferUsageFlags m_UsageFlags;
  VmaMemoryUsage m_MemoryUsage;
};

} // namespace mc
