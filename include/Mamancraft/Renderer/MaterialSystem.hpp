#pragma once

#include "Mamancraft/Renderer/GPUStructures.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanBuffer.hpp"
#include <memory>
#include <vector>

namespace mc {

class VulkanContext;

/// Manages an array of MaterialData on the GPU via SSBO.
///
/// Usage:
///   1. RegisterMaterial() for each block type at init
///   2. UploadToGPU() once after all materials are registered
///   3. Bind the SSBO in descriptor set for fragment shader access
///
/// Materials are immutable after upload (no per-frame updates needed).
class MaterialSystem {
public:
  explicit MaterialSystem(VulkanContext &context);
  ~MaterialSystem() = default;

  MaterialSystem(const MaterialSystem &) = delete;
  MaterialSystem &operator=(const MaterialSystem &) = delete;

  /// Register a material and return its index (materialID).
  /// Must be called BEFORE UploadToGPU().
  uint32_t RegisterMaterial(const MaterialData &data);

  /// Upload the material array to GPU SSBO. Call once after all
  /// RegisterMaterial() calls are done.
  void UploadToGPU();

  /// Return the number of registered materials.
  [[nodiscard]] uint32_t GetMaterialCount() const {
    return static_cast<uint32_t>(m_Materials.size());
  }

  /// Get the GPU buffer for descriptor binding.
  [[nodiscard]] vk::Buffer GetBuffer() const;

  /// Get descriptor info for binding this SSBO.
  [[nodiscard]] vk::DescriptorBufferInfo GetDescriptorInfo() const;

private:
  VulkanContext &m_Context;
  std::vector<MaterialData> m_Materials;
  std::unique_ptr<VulkanBuffer> m_Buffer;
  bool m_Uploaded = false;
};

} // namespace mc
