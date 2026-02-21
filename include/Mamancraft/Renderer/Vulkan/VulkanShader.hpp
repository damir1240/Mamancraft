#pragma once

#include "Mamancraft/Renderer/Vulkan/VulkanDevice.hpp"
#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

namespace mc {

class VulkanShader {
public:
  VulkanShader(const std::unique_ptr<VulkanDevice> &device,
               const std::string &filepath);
  ~VulkanShader();

  VulkanShader(const VulkanShader &) = delete;
  VulkanShader &operator=(const VulkanShader &) = delete;

  VkShaderModule GetShaderModule() const { return m_ShaderModule; }

private:
  std::vector<char> ReadFile(const std::string &filepath);
  void CreateShaderModule(const std::vector<char> &code);

private:
  const std::unique_ptr<VulkanDevice> &m_Device;
  VkShaderModule m_ShaderModule = VK_NULL_HANDLE;
};

} // namespace mc
