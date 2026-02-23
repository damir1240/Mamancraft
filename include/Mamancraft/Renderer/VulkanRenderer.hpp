#pragma once

#include "Mamancraft/Renderer/Vulkan/VulkanBuffer.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanFrameData.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanPipeline.hpp"
#include "Mamancraft/Renderer/VulkanContext.hpp"
#include <memory>
#include <vector>

namespace mc {

class VulkanMesh;

class VulkanRenderer {
public:
  VulkanRenderer(VulkanContext &context);
  ~VulkanRenderer();

  VulkanRenderer(const VulkanRenderer &) = delete;
  VulkanRenderer &operator=(const VulkanRenderer &) = delete;

  vk::CommandBuffer BeginFrame();
  void EndFrame();

  void BeginRenderPass(vk::CommandBuffer commandBuffer);
  void EndRenderPass(vk::CommandBuffer commandBuffer);

  void UpdateGlobalUbo(const GlobalUbo &ubo);

  void DrawMesh(vk::CommandBuffer commandBuffer, VulkanPipeline &pipeline,
                VulkanMesh &mesh, const PushConstantData &pushData);

  // --- Bindless Texture Support ---
  uint32_t RegisterTexture(const class VulkanTexture &texture);

  vk::DescriptorSetLayout GetGlobalDescriptorSetLayout() const {
    return m_GlobalDescriptorSetLayout;
  }
  vk::DescriptorSetLayout GetBindlessDescriptorSetLayout() const {
    return m_BindlessDescriptorSetLayout;
  }

private:
  void CreateSyncObjects();
  void CreateCommandBuffers();
  void CreateDescriptors();
  void CreateUboBuffers();

private:
  VulkanContext &m_Context;
  std::vector<std::unique_ptr<VulkanCommandBuffer>> m_CommandBuffers;

  std::vector<vk::Semaphore> m_ImageAvailableSemaphores;
  std::vector<vk::Semaphore> m_RenderFinishedSemaphores;
  std::vector<vk::Fence> m_InFlightFences;

  uint32_t m_CurrentFrameIndex = 0;
  uint32_t m_CurrentImageIndex = 0;
  bool m_IsFrameStarted = false;

  const int MAX_FRAMES_IN_FLIGHT = 2;
  const uint32_t MAX_BINDLESS_RESOURCES = 10000;

  // Descriptors & UBOs
  vk::DescriptorSetLayout m_GlobalDescriptorSetLayout = nullptr;
  vk::DescriptorSetLayout m_BindlessDescriptorSetLayout = nullptr;
  vk::DescriptorPool m_DescriptorPool = nullptr;

  std::vector<vk::DescriptorSet> m_GlobalDescriptorSets;
  vk::DescriptorSet m_BindlessDescriptorSet = nullptr;

  std::vector<std::unique_ptr<VulkanBuffer>> m_UboBuffers;
};

} // namespace mc
