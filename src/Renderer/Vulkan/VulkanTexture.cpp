#include "Mamancraft/Renderer/Vulkan/VulkanTexture.hpp"
#include "Mamancraft/Core/Logger.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanBuffer.hpp"
#include "Mamancraft/Renderer/VulkanContext.hpp"

#include <stb_image.h>
#include <stdexcept>

namespace mc {

VulkanTexture::VulkanTexture(VulkanContext &context,
                             const std::filesystem::path &filePath)
    : m_Device(context.GetDevice()) {
  int texWidth, texHeight, texChannels;
  stbi_uc *pixels = stbi_load(filePath.string().c_str(), &texWidth, &texHeight,
                              &texChannels, STBI_rgb_alpha);
  vk::DeviceSize imageSize = texWidth * texHeight * 4;

  if (!pixels) {
    MC_CRITICAL("Failed to load texture image: {0}", filePath.string());
    throw std::runtime_error("failed to load texture image!");
  }

  // Staging buffer
  VulkanBuffer stagingBuffer(
      context.GetAllocator()->GetAllocator(), imageSize, 1,
      vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_AUTO,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

  stagingBuffer.Map();
  stagingBuffer.WriteToBuffer(pixels);
  stagingBuffer.Unmap();

  stbi_image_free(pixels);

  m_Image = std::make_unique<VulkanImage>(
      context.GetAllocator(), context.GetDevice(),
      vk::Extent2D{static_cast<uint32_t>(texWidth),
                   static_cast<uint32_t>(texHeight)},
      vk::Format::eR8G8B8A8Srgb,
      vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
      vk::ImageAspectFlagBits::eColor);

  // Upload to GPU
  context.ImmediateSubmit([&](vk::CommandBuffer cmd) {
    m_Image->TransitionLayout(cmd, vk::ImageLayout::eUndefined,
                              vk::ImageLayout::eTransferDstOptimal);

    vk::BufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = vk::Offset3D{0, 0, 0};
    region.imageExtent = vk::Extent3D{static_cast<uint32_t>(texWidth),
                                      static_cast<uint32_t>(texHeight), 1};

    cmd.copyBufferToImage(stagingBuffer.GetBuffer(), m_Image->GetImage(),
                          vk::ImageLayout::eTransferDstOptimal, region);

    m_Image->TransitionLayout(cmd, vk::ImageLayout::eTransferDstOptimal,
                              vk::ImageLayout::eShaderReadOnlyOptimal);
  });

  CreateSampler(context.GetPhysicalDeviceProperties());
}

VulkanTexture::~VulkanTexture() {
  if (m_Sampler) {
    m_Device->GetLogicalDevice().destroySampler(m_Sampler);
  }
}

void VulkanTexture::CreateSampler(
    const vk::PhysicalDeviceProperties &properties) {
  vk::SamplerCreateInfo samplerInfo{};
  samplerInfo.magFilter = vk::Filter::eNearest; // Pixelated for voxels
  samplerInfo.minFilter = vk::Filter::eNearest;
  samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
  samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
  samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
  samplerInfo.anisotropyEnable = VK_TRUE;
  samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
  samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = vk::CompareOp::eAlways;
  samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;

  try {
    m_Sampler = m_Device->GetLogicalDevice().createSampler(samplerInfo);
  } catch (const vk::SystemError &e) {
    MC_CRITICAL("Failed to create texture sampler! Error: {}", e.what());
    throw std::runtime_error("failed to create texture sampler!");
  }
}

vk::DescriptorImageInfo VulkanTexture::GetDescriptorInfo() const {
  vk::DescriptorImageInfo imageInfo{};
  imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
  imageInfo.imageView = m_Image->GetImageView();
  imageInfo.sampler = m_Sampler;
  return imageInfo;
}

} // namespace mc
