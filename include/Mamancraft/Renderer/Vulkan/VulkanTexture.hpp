#pragma once
#include "Mamancraft/Renderer/Vulkan/VulkanCore.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanImage.hpp"
#include <filesystem>
#include <memory>

namespace mc {

class VulkanContext;
class VulkanDevice;
class VulkanAllocator;

class VulkanTexture {
public:
  VulkanTexture(VulkanContext &context, const std::filesystem::path &filePath);
  ~VulkanTexture();

  [[nodiscard]] vk::DescriptorImageInfo GetDescriptorInfo() const;
  [[nodiscard]] vk::ImageView GetImageView() const {
    return m_Image->GetImageView();
  }
  [[nodiscard]] vk::Sampler GetSampler() const { return m_Sampler; }

private:
  void CreateSampler(const vk::PhysicalDeviceProperties &properties);

private:
  const std::unique_ptr<VulkanDevice> &m_Device;
  std::unique_ptr<VulkanImage> m_Image;
  vk::Sampler m_Sampler;
};

} // namespace mc
