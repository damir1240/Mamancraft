#pragma once

#include "Mamancraft/Renderer/Vulkan/VulkanPipeline.hpp"
#include "Mamancraft/Renderer/VulkanContext.hpp"

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

namespace mc {

class VulkanRenderer {
public:
  VulkanRenderer(VulkanContext &context);
  ~VulkanRenderer();

  VulkanRenderer(const VulkanRenderer &) = delete;
  VulkanRenderer &operator=(const VulkanRenderer &) = delete;

  VkCommandBuffer BeginFrame();
  void EndFrame();

  void BeginRenderPass(VkCommandBuffer commandBuffer);
  void EndRenderPass(VkCommandBuffer commandBuffer);

  void Draw(VkCommandBuffer commandBuffer, VulkanPipeline &pipeline);

private:
  void CreateSyncObjects();
  void CreateCommandBuffers();

private:
  VulkanContext &m_Context;
  std::vector<std::unique_ptr<VulkanCommandBuffer>> m_CommandBuffers;

  std::vector<VkSemaphore> m_ImageAvailableSemaphores;
  std::vector<VkSemaphore> m_RenderFinishedSemaphores;
  std::vector<VkFence> m_InFlightFences;
  std::vector<VkFence> m_ImagesInFlight;

  uint32_t m_CurrentFrameIndex = 0;
  uint32_t m_CurrentImageIndex = 0;
  bool m_IsFrameStarted = false;

  const int MAX_FRAMES_IN_FLIGHT = 2;
};

} // namespace mc
