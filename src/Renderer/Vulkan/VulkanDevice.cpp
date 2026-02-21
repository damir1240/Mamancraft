#include "Mamancraft/Renderer/Vulkan/VulkanDevice.hpp"

namespace mc {

VulkanDevice::VulkanDevice(vk::PhysicalDevice physicalDevice,
                           vk::Device logicalDevice,
                           const QueueFamilyIndices &indices)
    : m_PhysicalDevice(physicalDevice), m_LogicalDevice(logicalDevice),
      m_Indices(indices) {

  m_GraphicsQueue =
      m_LogicalDevice.getQueue(m_Indices.graphicsFamily.value(), 0);
  m_PresentQueue = m_LogicalDevice.getQueue(m_Indices.presentFamily.value(), 0);
  m_ComputeQueue = m_LogicalDevice.getQueue(m_Indices.computeFamily.value(), 0);
}

VulkanDevice::~VulkanDevice() {
  if (m_LogicalDevice) {
    m_LogicalDevice.destroy();
  }
}

} // namespace mc
