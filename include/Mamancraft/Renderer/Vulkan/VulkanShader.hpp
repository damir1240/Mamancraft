#pragma once

#include "Mamancraft/Renderer/Vulkan/VulkanCore.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanDevice.hpp"
#include <memory>
#include <string>
#include <vector>


namespace mc {

class VulkanShader {
public:
  VulkanShader(const std::unique_ptr<VulkanDevice> &device,
               const std::string &filepath);
  ~VulkanShader();

  VulkanShader(const VulkanShader &) = delete;
  VulkanShader &operator=(const VulkanShader &) = delete;

  vk::ShaderModule GetShaderModule() const { return m_ShaderModule; }

private:
  std::vector<char> ReadFile(const std::string &filepath);
  void CreateShaderModule(const std::vector<char> &code);

private:
  const std::unique_ptr<VulkanDevice> &m_Device;
  vk::ShaderModule m_ShaderModule = nullptr;
};

} // namespace mc
