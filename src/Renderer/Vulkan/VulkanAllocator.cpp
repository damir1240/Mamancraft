#include "Mamancraft/Renderer/Vulkan/VulkanAllocator.hpp"
#include "Mamancraft/Core/Logger.hpp"

#include <stdexcept>

namespace mc {

VulkanAllocator::VulkanAllocator(vk::Instance instance,
                                 const std::unique_ptr<VulkanDevice> &device) {
  VmaAllocatorCreateInfo allocatorInfo{};
  allocatorInfo.physicalDevice =
      static_cast<VkPhysicalDevice>(device->GetPhysicalDevice());
  allocatorInfo.device = static_cast<VkDevice>(device->GetLogicalDevice());
  allocatorInfo.instance = static_cast<VkInstance>(instance);
  allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_4;

  if (vmaCreateAllocator(&allocatorInfo, &m_Allocator) != VK_SUCCESS) {
    MC_CRITICAL("Failed to create Vulkan Memory Allocator!");
    throw std::runtime_error("failed to create VMA!");
  }

  MC_INFO("Vulkan Memory Allocator (VMA) created successfully.");
}

VulkanAllocator::~VulkanAllocator() {
  if (m_Allocator) {
    MC_DEBUG("VulkanAllocator: Destroying VMA allocator instance {}", (void*)m_Allocator);
    
    // Check for memory leaks before destroying
    VmaTotalStatistics stats;
    vmaCalculateStatistics(m_Allocator, &stats);
    
    if (stats.total.statistics.allocationCount > 0) {
      MC_WARN("VulkanAllocator: {} allocations still exist before destroying allocator!", 
              stats.total.statistics.allocationCount);
      MC_WARN("VulkanAllocator: {} bytes still allocated", 
              stats.total.statistics.allocationBytes);
    }
    
    vmaDestroyAllocator(m_Allocator);
    m_Allocator = nullptr;
    MC_INFO("VulkanAllocator: VMA allocator destroyed successfully");
  }
}

} // namespace mc
