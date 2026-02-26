#include "Mamancraft/Renderer/MaterialSystem.hpp"
#include "Mamancraft/Core/Logger.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanBuffer.hpp"
#include "Mamancraft/Renderer/VulkanContext.hpp"

#include <stdexcept>

namespace mc {

MaterialSystem::MaterialSystem(VulkanContext &context) : m_Context(context) {
  // Reserve material 0 as a "default/missing" material (magenta debug)
  MaterialData defaultMat;
  defaultMat.albedoTint = glm::vec4(1.0f, 0.0f, 1.0f, 1.0f); // magenta
  defaultMat.albedoTexIndex = 0;
  defaultMat.animFrames = 1;
  defaultMat.animFPS = 0.0f;
  defaultMat.flags = 0;
  m_Materials.push_back(defaultMat);

  MC_INFO("MaterialSystem: Initialized with default material (ID=0)");
}

uint32_t MaterialSystem::RegisterMaterial(const MaterialData &data) {
  if (m_Uploaded) {
    MC_ERROR("MaterialSystem: Cannot register materials after GPU upload!");
    throw std::runtime_error(
        "MaterialSystem: RegisterMaterial called after UploadToGPU");
  }

  uint32_t id = static_cast<uint32_t>(m_Materials.size());
  m_Materials.push_back(data);

  MC_DEBUG("MaterialSystem: Registered material ID={} (texIdx={}, "
           "animFrames={}, flags={})",
           id, data.albedoTexIndex, data.animFrames, data.flags);

  return id;
}

void MaterialSystem::UploadToGPU() {
  if (m_Materials.empty()) {
    MC_WARN("MaterialSystem: No materials to upload");
    return;
  }

  vk::DeviceSize bufferSize = sizeof(MaterialData) * m_Materials.size();

  // Create a staging buffer (CPU-visible)
  VulkanBuffer stagingBuffer(
      m_Context.GetAllocator()->GetAllocator(), bufferSize, 1,
      vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_AUTO,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

  stagingBuffer.Map();
  stagingBuffer.WriteToBuffer(m_Materials.data(), bufferSize);
  stagingBuffer.Unmap();

  // Create device-local SSBO
  m_Buffer = std::make_unique<VulkanBuffer>(
      m_Context.GetAllocator()->GetAllocator(), bufferSize, 1,
      vk::BufferUsageFlagBits::eStorageBuffer |
          vk::BufferUsageFlagBits::eTransferDst,
      VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);

  // Copy staging → device-local
  VulkanBuffer::CopyBuffer(m_Context, stagingBuffer.GetBuffer(),
                           m_Buffer->GetBuffer(), bufferSize);

  m_Uploaded = true;

  MC_INFO("MaterialSystem: Uploaded {} materials ({} bytes) to GPU SSBO",
          m_Materials.size(), bufferSize);
}

vk::Buffer MaterialSystem::GetBuffer() const {
  if (!m_Buffer) {
    throw std::runtime_error(
        "MaterialSystem: GetBuffer called before UploadToGPU");
  }
  return m_Buffer->GetBuffer();
}

vk::DescriptorBufferInfo MaterialSystem::GetDescriptorInfo() const {
  vk::DescriptorBufferInfo info;
  info.buffer = GetBuffer();
  info.offset = 0;
  info.range = sizeof(MaterialData) * m_Materials.size();
  return info;
}

} // namespace mc
