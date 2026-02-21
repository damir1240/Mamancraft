#pragma once

#include "Mamancraft/Renderer/Vulkan/VulkanCore.hpp"
#include <cstdint>
#include <optional>


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
  VulkanDevice(vk::PhysicalDevice physicalDevice, vk::Device logicalDevice,
               const QueueFamilyIndices &indices);
  ~VulkanDevice();

  vk::PhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
  vk::Device GetLogicalDevice() const { return m_LogicalDevice; }

  vk::Queue GetGraphicsQueue() const { return m_GraphicsQueue; }
  vk::Queue GetPresentQueue() const { return m_PresentQueue; }
  vk::Queue GetComputeQueue() const { return m_ComputeQueue; }

  const QueueFamilyIndices &GetQueueFamilyIndices() const { return m_Indices; }

private:
  vk::PhysicalDevice m_PhysicalDevice;
  vk::Device m_LogicalDevice;

  vk::Queue m_GraphicsQueue;
  vk::Queue m_PresentQueue;
  vk::Queue m_ComputeQueue;

  QueueFamilyIndices m_Indices;
};

} // namespace mc
