#include "Mamancraft/Renderer/Vulkan/VulkanImage.hpp"
#include "Mamancraft/Core/Logger.hpp"

namespace mc {

VulkanImage::VulkanImage(const std::unique_ptr<VulkanAllocator> &allocator,
                         const std::unique_ptr<VulkanDevice> &device,
                         vk::Extent2D extent, vk::Format format,
                         vk::ImageUsageFlags usage, vk::ImageAspectFlags aspect)
    : m_Allocator(allocator), m_Device(device), m_Format(format) {

  vk::ImageCreateInfo imageInfo{};
  imageInfo.imageType = vk::ImageType::e2D;
  imageInfo.extent.width = extent.width;
  imageInfo.extent.height = extent.height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format = format;
  imageInfo.tiling = vk::ImageTiling::eOptimal;
  imageInfo.initialLayout = vk::ImageLayout::eUndefined;
  imageInfo.usage = usage;
  imageInfo.samples = vk::SampleCountFlagBits::e1;
  imageInfo.sharingMode = vk::SharingMode::eExclusive;

  VmaAllocationCreateInfo allocInfo{};
  allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

  VkImage cImage;
  if (vmaCreateImage(m_Allocator->GetAllocator(),
                     reinterpret_cast<VkImageCreateInfo *>(&imageInfo),
                     &allocInfo, &cImage, &m_Allocation,
                     nullptr) != VK_SUCCESS) {
    MC_CRITICAL("Failed to create VMA image!");
    throw std::runtime_error("failed to create image!");
  }
  m_Image = cImage;

  vk::ImageViewCreateInfo viewInfo{};
  viewInfo.image = m_Image;
  viewInfo.viewType = vk::ImageViewType::e2D;
  viewInfo.format = format;
  viewInfo.subresourceRange.aspectMask = aspect;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  try {
    m_ImageView = m_Device->GetLogicalDevice().createImageView(viewInfo);
  } catch (const vk::SystemError &e) {
    MC_CRITICAL("Failed to create image view! Error: {}", e.what());
    throw std::runtime_error("failed to create image view!");
  }
}

VulkanImage::~VulkanImage() {
  if (m_ImageView) {
    m_Device->GetLogicalDevice().destroyImageView(m_ImageView);
  }
  if (m_Image) {
    vmaDestroyImage(m_Allocator->GetAllocator(), m_Image, m_Allocation);
  }
}

void VulkanImage::TransitionLayout(vk::CommandBuffer cmd,
                                   vk::ImageLayout oldLayout,
                                   vk::ImageLayout newLayout) {
  vk::ImageMemoryBarrier barrier{};
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = m_Image;
  barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  vk::PipelineStageFlags sourceStage;
  vk::PipelineStageFlags destinationStage;

  if (oldLayout == vk::ImageLayout::eUndefined &&
      newLayout == vk::ImageLayout::eTransferDstOptimal) {
    barrier.srcAccessMask = vk::AccessFlags{};
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
    sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
    destinationStage = vk::PipelineStageFlagBits::eTransfer;
  } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
             newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
    sourceStage = vk::PipelineStageFlagBits::eTransfer;
    destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
  } else {
    throw std::invalid_argument("unsupported layout transition!");
  }

  cmd.pipelineBarrier(sourceStage, destinationStage, vk::DependencyFlags{},
                      nullptr, nullptr, barrier);
}

} // namespace mc
