#include "Mamancraft/Renderer/Vulkan/VulkanBuffer.hpp"
#include "Mamancraft/Core/Logger.hpp"
#include <cstring>
#include <stdexcept>

namespace mc {

vk::DeviceSize VulkanBuffer::GetAlignment(vk::DeviceSize instanceSize,
                                          vk::DeviceSize minOffsetAlignment) {
  if (minOffsetAlignment > 0) {
    return (instanceSize + minOffsetAlignment - 1) & ~(minOffsetAlignment - 1);
  }
  return instanceSize;
}

VulkanBuffer::VulkanBuffer(VmaAllocator allocator, vk::DeviceSize instanceSize,
                           uint32_t instanceCount,
                           vk::BufferUsageFlags usageFlags,
                           VmaMemoryUsage memoryUsage,
                           vk::DeviceSize minOffsetAlignment)
    : m_Allocator(allocator), m_InstanceSize(instanceSize),
      m_InstanceCount(instanceCount), m_UsageFlags(usageFlags),
      m_MemoryUsage(memoryUsage) {

  m_AlignmentSize = GetAlignment(instanceSize, minOffsetAlignment);
  m_BufferSize = m_AlignmentSize * instanceCount;

  vk::BufferCreateInfo bufferInfo;
  bufferInfo.size = m_BufferSize;
  bufferInfo.usage = usageFlags;
  bufferInfo.sharingMode = vk::SharingMode::eExclusive;

  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = memoryUsage;

  VkBufferCreateInfo vkBufferInfo = static_cast<VkBufferCreateInfo>(bufferInfo);
  VkBuffer vkBuffer;

  if (vmaCreateBuffer(m_Allocator, &vkBufferInfo, &allocInfo, &vkBuffer,
                      &m_Allocation, &m_AllocationInfo) != VK_SUCCESS) {
    MC_CRITICAL("Failed to create VulkanBuffer via VMA");
    throw std::runtime_error("failed to create buffer!");
  }

  m_Buffer = vkBuffer;
}

VulkanBuffer::~VulkanBuffer() {
  Unmap();
  if (m_Buffer && m_Allocation) {
    vmaDestroyBuffer(m_Allocator, static_cast<VkBuffer>(m_Buffer),
                     m_Allocation);
  }
}

vk::Result VulkanBuffer::Map() {
  if (vmaMapMemory(m_Allocator, m_Allocation, &m_MappedRegion) != VK_SUCCESS) {
    return vk::Result::eErrorMemoryMapFailed;
  }
  return vk::Result::eSuccess;
}

void VulkanBuffer::Unmap() {
  if (m_MappedRegion) {
    vmaUnmapMemory(m_Allocator, m_Allocation);
    m_MappedRegion = nullptr;
  }
}

void VulkanBuffer::WriteToBuffer(void *data, vk::DeviceSize size,
                                 vk::DeviceSize offset) {
  if (size == VK_WHOLE_SIZE) {
    memcpy(m_MappedRegion, data, m_BufferSize);
  } else {
    char *memOffset = (char *)m_MappedRegion;
    memOffset += offset;
    memcpy(memOffset, data, size);
  }
}

vk::Result VulkanBuffer::Flush(vk::DeviceSize size, vk::DeviceSize offset) {
  if (vmaFlushAllocation(m_Allocator, m_Allocation, offset, size) !=
      VK_SUCCESS) {
    return vk::Result::eErrorUnknown;
  }
  return vk::Result::eSuccess;
}

vk::Result VulkanBuffer::Invalidate(vk::DeviceSize size,
                                    vk::DeviceSize offset) {
  if (vmaInvalidateAllocation(m_Allocator, m_Allocation, offset, size) !=
      VK_SUCCESS) {
    return vk::Result::eErrorUnknown;
  }
  return vk::Result::eSuccess;
}

void VulkanBuffer::CopyBuffer(VulkanContext &context, vk::Buffer srcBuffer,
                              vk::Buffer dstBuffer, vk::DeviceSize size) {
  context.ImmediateSubmit([=](vk::CommandBuffer commandBuffer) {
    vk::BufferCopy copyRegion;
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;

    commandBuffer.copyBuffer(srcBuffer, dstBuffer, 1, &copyRegion);
  });
}

} // namespace mc
