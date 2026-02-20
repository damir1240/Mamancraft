#pragma once

#include <cstdint>
#include <optional>
#include <vulkan/vulkan.h>

namespace mc {

struct QueueFamilyIndices {
  std::optional<uint32_t> graphicsFamily;
  std::optional<uint32_t> presentFamily;
  std::optional<uint32_t> computeFamily;

  bool isComplete() const {
    return graphicsFamily.has_value() && presentFamily.has_value() &&
           computeFamily.has_value();
  }
};

class VulkanDevice {
public:
  VulkanDevice(VkPhysicalDevice physicalDevice, VkDevice logicalDevice,
               const QueueFamilyIndices &indices);
  ~VulkanDevice();

  VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
  VkDevice GetLogicalDevice() const { return m_LogicalDevice; }

  VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
  VkQueue GetPresentQueue() const { return m_PresentQueue; }
  VkQueue GetComputeQueue() const { return m_ComputeQueue; }

  const QueueFamilyIndices &GetQueueFamilyIndices() const { return m_Indices; }

private:
  VkPhysicalDevice m_PhysicalDevice;
  VkDevice m_LogicalDevice;

  VkQueue m_GraphicsQueue;
  VkQueue m_PresentQueue;
  VkQueue m_ComputeQueue;

  QueueFamilyIndices m_Indices;
};

} // namespace mc
