#include "Mamancraft/Renderer/Vulkan/VulkanAllocator.hpp"
#include "Mamancraft/Core/Logger.hpp"

#include <stdexcept>

namespace mc {

VulkanAllocator::VulkanAllocator(VkInstance instance,
                                 const std::unique_ptr<VulkanDevice> &device) {
  VmaAllocatorCreateInfo allocatorInfo{};
  allocatorInfo.physicalDevice = device->GetPhysicalDevice();
  allocatorInfo.device = device->GetLogicalDevice();
  allocatorInfo.instance = instance;
  allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_0;

  if (vmaCreateAllocator(&allocatorInfo, &m_Allocator) != VK_SUCCESS) {
    MC_CRITICAL("Failed to create Vulkan Memory Allocator!");
    throw std::runtime_error("failed to create VMA!");
  }

  MC_INFO("Vulkan Memory Allocator (VMA) created successfully.");
}

VulkanAllocator::~VulkanAllocator() {
  if (m_Allocator != VK_NULL_HANDLE) {
    vmaDestroyAllocator(m_Allocator);
    MC_INFO("Vulkan Memory Allocator (VMA) destroyed.");
  }
}

} // namespace mc
