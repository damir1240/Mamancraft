#include "Mamancraft/Renderer/VulkanRenderer.hpp"
#include "Mamancraft/Core/Logger.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanCommandPool.hpp"

#include <stdexcept>

namespace mc {

VulkanRenderer::VulkanRenderer(VulkanContext &context) : m_Context(context) {
  CreateCommandBuffers();
  CreateSyncObjects();
}

VulkanRenderer::~VulkanRenderer() {
  VkDevice device = m_Context.GetDevice()->GetLogicalDevice();

  vkDeviceWaitIdle(device);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vkDestroySemaphore(device, m_ImageAvailableSemaphores[i], nullptr);
    vkDestroyFence(device, m_InFlightFences[i], nullptr);
  }

  for (auto sem : m_RenderFinishedSemaphores) {
    vkDestroySemaphore(device, sem, nullptr);
  }
}

void VulkanRenderer::CreateCommandBuffers() {
  m_CommandBuffers = m_Context.GetCommandPool()->AllocateCommandBuffers(
      MAX_FRAMES_IN_FLIGHT, true);
}

void VulkanRenderer::CreateSyncObjects() {
  m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
  uint32_t imageCount = m_Context.GetSwapchain()->GetImages().size();
  m_RenderFinishedSemaphores.resize(imageCount);
  m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
  m_ImagesInFlight.resize(imageCount, VK_NULL_HANDLE);

  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  VkDevice device = m_Context.GetDevice()->GetLogicalDevice();

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    if (vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                          &m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
        vkCreateFence(device, &fenceInfo, nullptr, &m_InFlightFences[i]) !=
            VK_SUCCESS) {
      MC_CRITICAL("Failed to create Vulkan Sync Objects!");
      throw std::runtime_error("failed to create sync objects for a frame!");
    }
  }

  for (size_t i = 0; i < imageCount; i++) {
    if (vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                          &m_RenderFinishedSemaphores[i]) != VK_SUCCESS) {
      MC_CRITICAL("Failed to create Vulkan RenderFinished Semaphores!");
      throw std::runtime_error("failed to create sync objects for a frame!");
    }
  }
}

VkCommandBuffer VulkanRenderer::BeginFrame() {
  if (m_IsFrameStarted) {
    MC_WARN("Cannot call BeginFrame while already in progress");
    return VK_NULL_HANDLE;
  }

  VkDevice device = m_Context.GetDevice()->GetLogicalDevice();
  VkSwapchainKHR swapchain = m_Context.GetSwapchain()->GetSwapchain();

  vkWaitForFences(device, 1, &m_InFlightFences[m_CurrentFrameIndex], VK_TRUE,
                  UINT64_MAX);

  VkResult result =
      vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                            m_ImageAvailableSemaphores[m_CurrentFrameIndex],
                            VK_NULL_HANDLE, &m_CurrentImageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    // Requires swapchain recreation.
    m_Context.GetSwapchain()->Recreate();
    return VK_NULL_HANDLE;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    MC_CRITICAL("Failed to acquire swapchain image!");
    throw std::runtime_error("failed to acquire swapchain image!");
  }

  // Check if a previous frame is using this image (i.e. there is its fence to
  // wait on)
  if (m_ImagesInFlight[m_CurrentImageIndex] != VK_NULL_HANDLE) {
    vkWaitForFences(device, 1, &m_ImagesInFlight[m_CurrentImageIndex], VK_TRUE,
                    UINT64_MAX);
  }

  // Mark the image as now being in use by this frame
  m_ImagesInFlight[m_CurrentImageIndex] = m_InFlightFences[m_CurrentFrameIndex];

  vkResetFences(device, 1, &m_InFlightFences[m_CurrentFrameIndex]);

  auto commandBuffer =
      m_CommandBuffers[m_CurrentFrameIndex]->GetCommandBuffer();

  // Begin command buffer
  m_CommandBuffers[m_CurrentFrameIndex]->Reset();
  m_CommandBuffers[m_CurrentFrameIndex]->Begin();

  m_IsFrameStarted = true;
  return commandBuffer;
}

void VulkanRenderer::EndFrame() {
  if (!m_IsFrameStarted) {
    MC_WARN("Cannot call EndFrame while frame is not in progress");
    return;
  }

  auto commandBuffer =
      m_CommandBuffers[m_CurrentFrameIndex]->GetCommandBuffer();

  // End command buffer
  m_CommandBuffers[m_CurrentFrameIndex]->End();

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  VkSemaphore waitSemaphores[] = {
      m_ImageAvailableSemaphores[m_CurrentFrameIndex]};
  VkPipelineStageFlags waitStages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;

  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  VkSemaphore signalSemaphores[] = {
      m_RenderFinishedSemaphores[m_CurrentImageIndex]};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  if (vkQueueSubmit(m_Context.GetDevice()->GetGraphicsQueue(), 1, &submitInfo,
                    m_InFlightFences[m_CurrentFrameIndex]) != VK_SUCCESS) {
    MC_CRITICAL("Failed to submit draw command buffer!");
    throw std::runtime_error("failed to submit draw command buffer!");
  }

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;

  VkSwapchainKHR swapchains[] = {m_Context.GetSwapchain()->GetSwapchain()};
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapchains;
  presentInfo.pImageIndices = &m_CurrentImageIndex;

  VkResult result =
      vkQueuePresentKHR(m_Context.GetDevice()->GetPresentQueue(), &presentInfo);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    m_Context.GetSwapchain()->Recreate();
  } else if (result != VK_SUCCESS) {
    MC_CRITICAL("Failed to present swapchain image!");
    throw std::runtime_error("failed to present swapchain image!");
  }

  m_IsFrameStarted = false;
  m_CurrentFrameIndex = (m_CurrentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::BeginRenderPass(VkCommandBuffer commandBuffer) {
  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = m_Context.GetRenderPass()->GetRenderPass();

  if (m_CurrentImageIndex >= m_Context.GetFramebuffers().size()) {
    MC_CRITICAL("Image index {} out of framebuffers range {}",
                m_CurrentImageIndex, m_Context.GetFramebuffers().size());
    return;
  }

  renderPassInfo.framebuffer =
      m_Context.GetFramebuffers()[m_CurrentImageIndex]->GetFramebuffer();

  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = m_Context.GetSwapchain()->GetExtent();

  VkClearValue clearColor = {{{0.01f, 0.01f, 0.01f, 1.0f}}};
  renderPassInfo.clearValueCount = 1;
  renderPassInfo.pClearValues = &clearColor;

  vkCmdBeginRenderPass(commandBuffer, &renderPassInfo,
                       VK_SUBPASS_CONTENTS_INLINE);

  // Set Viewport and Scissor for dynamic states
  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width =
      static_cast<float>(m_Context.GetSwapchain()->GetExtent().width);
  viewport.height =
      static_cast<float>(m_Context.GetSwapchain()->GetExtent().height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = m_Context.GetSwapchain()->GetExtent();
  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

void VulkanRenderer::EndRenderPass(VkCommandBuffer commandBuffer) {
  vkCmdEndRenderPass(commandBuffer);
}

void VulkanRenderer::Draw(VkCommandBuffer commandBuffer,
                          VulkanPipeline &pipeline) {
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline.GetPipeline());
  vkCmdDraw(commandBuffer, 3, 1, 0, 0); // 3 vertices, 1 instance
}

} // namespace mc
