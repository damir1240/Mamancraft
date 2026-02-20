#include "Mamancraft/Renderer/Vulkan/VulkanDevice.hpp"

namespace mc {

VulkanDevice::VulkanDevice(VkPhysicalDevice physicalDevice,
                           VkDevice logicalDevice,
                           const QueueFamilyIndices &indices)
    : m_PhysicalDevice(physicalDevice), m_LogicalDevice(logicalDevice),
      m_Indices(indices) {

  vkGetDeviceQueue(m_LogicalDevice, m_Indices.graphicsFamily.value(), 0,
                   &m_GraphicsQueue);
  vkGetDeviceQueue(m_LogicalDevice, m_Indices.presentFamily.value(), 0,
                   &m_PresentQueue);
  vkGetDeviceQueue(m_LogicalDevice, m_Indices.computeFamily.value(), 0,
                   &m_ComputeQueue);
}

} // namespace mc
