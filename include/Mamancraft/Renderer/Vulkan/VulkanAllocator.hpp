#pragma once

#include "Mamancraft/Renderer/Vulkan/VulkanDevice.hpp"
#include <memory>
#include <vk_mem_alloc.h>


namespace mc {

class VulkanAllocator {
public:
  VulkanAllocator(VkInstance instance,
                  const std::unique_ptr<VulkanDevice> &device);
  ~VulkanAllocator();

  VulkanAllocator(const VulkanAllocator &) = delete;
  VulkanAllocator &operator=(const VulkanAllocator &) = delete;

  VmaAllocator GetAllocator() const { return m_Allocator; }

private:
  VmaAllocator m_Allocator = VK_NULL_HANDLE;
};

} // namespace mc
