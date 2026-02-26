#pragma once

#include "Mamancraft/Renderer/GPUStructures.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanBuffer.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanCore.hpp"
#include <memory>

namespace mc {

class VulkanContext;

/// GPU Frustum Culling System.
///
/// Uses a compute shader to test each chunk's AABB against the camera's
/// frustum planes. Culled chunks have their DrawCommand.instanceCount set
/// to 0, preventing them from being drawn by vkCmdDrawIndexedIndirect.
///
/// Pipeline:
///   1. CPU: Update CullUniforms (viewProj, frustum planes, draw count)
///   2. GPU: Dispatch compute shader (1 thread per draw command)
///   3. GPU: Pipeline barrier (compute write → indirect draw read)
///   4. GPU: vkCmdDrawIndexedIndirect
class CullingSystem {
public:
  CullingSystem(VulkanContext &context);
  ~CullingSystem();

  CullingSystem(const CullingSystem &) = delete;
  CullingSystem &operator=(const CullingSystem &) = delete;

  /// Update culling uniforms and dispatch compute shader.
  /// Must be called after IndirectDrawSystem::FlushToGPU() and before
  /// the draw call.
  void Execute(vk::CommandBuffer commandBuffer, const CullUniforms &uniforms,
               vk::Buffer drawCommandBuffer,
               vk::DeviceSize drawCommandBufferSize,
               vk::Buffer objectDataBuffer, vk::DeviceSize objectDataBufferSize,
               uint32_t drawCount);

  /// Get the descriptor set layout for the culling pipeline.
  [[nodiscard]] vk::DescriptorSetLayout GetDescriptorSetLayout() const {
    return m_DescriptorSetLayout;
  }

private:
  void CreateDescriptorSetLayout();
  void CreatePipeline();
  void CreateDescriptorPool();

  VulkanContext &m_Context;

  vk::Pipeline m_Pipeline = nullptr;
  vk::PipelineLayout m_PipelineLayout = nullptr;
  vk::DescriptorSetLayout m_DescriptorSetLayout = nullptr;
  vk::DescriptorPool m_DescriptorPool = nullptr;
  vk::DescriptorSet m_DescriptorSet = nullptr;

  // UBO for CullUniforms
  std::unique_ptr<VulkanBuffer> m_UniformBuffer;
};

} // namespace mc
