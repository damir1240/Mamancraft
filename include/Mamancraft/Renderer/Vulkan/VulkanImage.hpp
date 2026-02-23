#pragma once

#include "Mamancraft/Renderer/Vulkan/VulkanAllocator.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanCore.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanDevice.hpp"

namespace mc {

class VulkanImage {
public:
  VulkanImage(const std::unique_ptr<VulkanAllocator> &allocator,
              const std::unique_ptr<VulkanDevice> &device, vk::Extent2D extent,
              vk::Format format, vk::ImageUsageFlags usage,
              vk::ImageAspectFlags aspect);
  ~VulkanImage();

  VulkanImage(const VulkanImage &) = delete;
  VulkanImage &operator=(const VulkanImage &) = delete;

  vk::Image GetImage() const { return m_Image; }
  vk::ImageView GetImageView() const { return m_ImageView; }
  vk::Format GetFormat() const { return m_Format; }

  void TransitionLayout(vk::CommandBuffer cmd, vk::ImageLayout oldLayout,
                        vk::ImageLayout newLayout);

private:
  const std::unique_ptr<VulkanAllocator> &m_Allocator;
  const std::unique_ptr<VulkanDevice> &m_Device;

  vk::Image m_Image = nullptr;
  VmaAllocation m_Allocation = nullptr;
  vk::ImageView m_ImageView = nullptr;
  vk::Format m_Format;
};

} // namespace mc
