#pragma once

#include "Mamancraft/Renderer/Vulkan/VulkanPipeline.hpp"
#include "Mamancraft/Renderer/VulkanContext.hpp"

#include "Mamancraft/Renderer/Vulkan/VulkanCore.hpp"
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

  void Draw(vk::CommandBuffer commandBuffer, VulkanPipeline &pipeline,
            vk::Buffer vertexBuffer, vk::Buffer indexBuffer,
            uint32_t indexCount);

  void DrawMesh(vk::CommandBuffer commandBuffer, VulkanPipeline &pipeline,
                VulkanMesh &mesh);

private:
  void CreateSyncObjects();
  void CreateCommandBuffers();

private:
  VulkanContext &m_Context;
  std::vector<std::unique_ptr<VulkanCommandBuffer>> m_CommandBuffers;

  std::vector<vk::Semaphore> m_ImageAvailableSemaphores;
  std::vector<vk::Semaphore> m_RenderFinishedSemaphores;
  std::vector<vk::Fence> m_InFlightFences;
  std::vector<vk::Fence> m_ImagesInFlight;

  uint32_t m_CurrentFrameIndex = 0;
  uint32_t m_CurrentImageIndex = 0;
  bool m_IsFrameStarted = false;

  const int MAX_FRAMES_IN_FLIGHT = 2;
};

} // namespace mc
