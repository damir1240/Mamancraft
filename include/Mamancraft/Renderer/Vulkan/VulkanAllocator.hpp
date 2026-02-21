#pragma once

#include "Mamancraft/Renderer/Vulkan/VulkanCore.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanDevice.hpp"
#include <memory>

namespace mc {

class VulkanAllocator {
public:
  VulkanAllocator(vk::Instance instance,
                  const std::unique_ptr<VulkanDevice> &device);
  ~VulkanAllocator();

  VulkanAllocator(const VulkanAllocator &) = delete;
  VulkanAllocator &operator=(const VulkanAllocator &) = delete;

  VmaAllocator GetAllocator() const { return m_Allocator; }

private:
  VmaAllocator m_Allocator = nullptr;
};

} // namespace mc
