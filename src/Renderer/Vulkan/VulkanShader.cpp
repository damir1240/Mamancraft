#include "Mamancraft/Renderer/Vulkan/VulkanShader.hpp"
#include "Mamancraft/Core/Logger.hpp"
#include <fstream>
#include <stdexcept>

namespace mc {

VulkanShader::VulkanShader(const std::unique_ptr<VulkanDevice> &device,
                           const std::string &filepath)
    : m_Device(device) {
  MC_INFO("Loading shader: {0}", filepath);
  auto code = ReadFile(filepath);
  CreateShaderModule(code);
}

VulkanShader::~VulkanShader() {
  if (m_ShaderModule) {
    m_Device->GetLogicalDevice().destroyShaderModule(m_ShaderModule);
    m_ShaderModule = nullptr;
  }
}

std::vector<char> VulkanShader::ReadFile(const std::string &filepath) {
  std::ifstream file(filepath, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    MC_ERROR("Failed to open shader file: {0}", filepath);
    throw std::runtime_error("failed to open shader file: " + filepath);
  }

  size_t fileSize = (size_t)file.tellg();
  std::vector<char> buffer(fileSize);

  file.seekg(0);
  file.read(buffer.data(), fileSize);
  file.close();

  return buffer;
}

void VulkanShader::CreateShaderModule(const std::vector<char> &code) {
  vk::ShaderModuleCreateInfo createInfo;
  createInfo.codeSize = code.size();
  createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

  try {
    m_ShaderModule =
        m_Device->GetLogicalDevice().createShaderModule(createInfo);
  } catch (const vk::SystemError &e) {
    MC_CRITICAL("Failed to create Vulkan Shader Module! Error: {}", e.what());
    throw std::runtime_error("failed to create shader module!");
  }
}

} // namespace mc
