#include "Mamancraft/Renderer/VulkanRenderer.hpp"
#include "Mamancraft/Core/Logger.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanCommandPool.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanMesh.hpp"

#include <stdexcept>

namespace mc {

VulkanRenderer::VulkanRenderer(VulkanContext &context) : m_Context(context) {
  CreateCommandBuffers();
  CreateSyncObjects();
}

VulkanRenderer::~VulkanRenderer() {
  MC_DEBUG("VulkanRenderer destructor: Starting cleanup");
  
  vk::Device device = m_Context.GetDevice()->GetLogicalDevice();

  // Wait for all operations to complete before cleanup
  MC_DEBUG("VulkanRenderer: Waiting for device idle...");
  device.waitIdle();

  // Explicitly clear command buffers BEFORE destroying sync objects
  // This ensures proper destruction order and prevents heap corruption
  MC_DEBUG("VulkanRenderer: Clearing {} command buffers", m_CommandBuffers.size());
  m_CommandBuffers.clear();

  // Destroy synchronization primitives
  MC_DEBUG("VulkanRenderer: Destroying {} semaphores and {} fences", 
           m_ImageAvailableSemaphores.size() + m_RenderFinishedSemaphores.size(),
           m_InFlightFences.size());
           
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    if (m_ImageAvailableSemaphores[i]) {
      device.destroySemaphore(m_ImageAvailableSemaphores[i]);
    }
    if (m_InFlightFences[i]) {
      device.destroyFence(m_InFlightFences[i]);
    }
  }

  for (auto sem : m_RenderFinishedSemaphores) {
    if (sem) {
      device.destroySemaphore(sem);
    }
  }
  
  MC_DEBUG("VulkanRenderer destructor: Cleanup completed");
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
  m_ImagesInFlight.resize(imageCount, nullptr);

  vk::SemaphoreCreateInfo semaphoreInfo;
  vk::FenceCreateInfo fenceInfo;
  fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

  vk::Device device = m_Context.GetDevice()->GetLogicalDevice();

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    try {
      m_ImageAvailableSemaphores[i] = device.createSemaphore(semaphoreInfo);
      m_InFlightFences[i] = device.createFence(fenceInfo);
    } catch (const vk::SystemError &e) {
      MC_CRITICAL("Failed to create Vulkan Sync Objects! Error: {}", e.what());
      throw std::runtime_error("failed to create sync objects for a frame!");
    }
  }

  for (size_t i = 0; i < imageCount; i++) {
    try {
      m_RenderFinishedSemaphores[i] = device.createSemaphore(semaphoreInfo);
    } catch (const vk::SystemError &e) {
      MC_CRITICAL(
          "Failed to create Vulkan RenderFinished Semaphores! Error: {}",
          e.what());
      throw std::runtime_error("failed to create sync objects for a frame!");
    }
  }
}

vk::CommandBuffer VulkanRenderer::BeginFrame() {
  if (m_IsFrameStarted) {
    MC_WARN("Cannot call BeginFrame while already in progress");
    return nullptr;
  }

  vk::Device device = m_Context.GetDevice()->GetLogicalDevice();
  vk::SwapchainKHR swapchain = m_Context.GetSwapchain()->GetSwapchain();

  if (device.waitForFences(1, &m_InFlightFences[m_CurrentFrameIndex], VK_TRUE,
                           UINT64_MAX) != vk::Result::eSuccess) {
    MC_CRITICAL("Wait for fences failed");
  }

  vk::Result result;
  try {
    auto acquireResult = device.acquireNextImageKHR(
        swapchain, UINT64_MAX, m_ImageAvailableSemaphores[m_CurrentFrameIndex],
        nullptr);
    result = acquireResult.result;
    m_CurrentImageIndex = acquireResult.value;
  } catch (const vk::OutOfDateKHRError &) {
    result = vk::Result::eErrorOutOfDateKHR;
  } catch (const vk::SystemError &e) {
    MC_CRITICAL("Failed to acquire swapchain image! Error: {}", e.what());
    throw std::runtime_error("failed to acquire swapchain image!");
  }

  if (result == vk::Result::eErrorOutOfDateKHR) {
    // Requires swapchain recreation.
    m_Context.GetSwapchain()->Recreate();
    return nullptr;
  } else if (result != vk::Result::eSuccess &&
             result != vk::Result::eSuboptimalKHR) {
    MC_CRITICAL("Failed to acquire swapchain image!");
    throw std::runtime_error("failed to acquire swapchain image!");
  }

  // Check if a previous frame is using this image (i.e. there is its fence to
  // wait on)
  if (m_ImagesInFlight[m_CurrentImageIndex]) {
    if (device.waitForFences(1, &m_ImagesInFlight[m_CurrentImageIndex], VK_TRUE,
                             UINT64_MAX) != vk::Result::eSuccess) {
      MC_CRITICAL("Wait for fences (image in flight) failed");
    }
  }

  // Mark the image as now being in use by this frame
  m_ImagesInFlight[m_CurrentImageIndex] = m_InFlightFences[m_CurrentFrameIndex];

  (void)device.resetFences(1, &m_InFlightFences[m_CurrentFrameIndex]);

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

  vk::SubmitInfo submitInfo;

  vk::Semaphore waitSemaphores[] = {
      m_ImageAvailableSemaphores[m_CurrentFrameIndex]};
  vk::PipelineStageFlags waitStages[] = {
      vk::PipelineStageFlagBits::eColorAttachmentOutput};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;

  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vk::Semaphore signalSemaphores[] = {
      m_RenderFinishedSemaphores[m_CurrentImageIndex]};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  try {
    m_Context.GetDevice()->GetGraphicsQueue().submit(
        submitInfo, m_InFlightFences[m_CurrentFrameIndex]);
  } catch (const vk::SystemError &e) {
    MC_CRITICAL("Failed to submit draw command buffer! Error: {}", e.what());
    throw std::runtime_error("failed to submit draw command buffer!");
  }

  vk::PresentInfoKHR presentInfo;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;

  vk::SwapchainKHR swapchains[] = {m_Context.GetSwapchain()->GetSwapchain()};
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapchains;
  presentInfo.pImageIndices = &m_CurrentImageIndex;

  vk::Result result;
  try {
    result = m_Context.GetDevice()->GetPresentQueue().presentKHR(presentInfo);
  } catch (const vk::OutOfDateKHRError &) {
    result = vk::Result::eErrorOutOfDateKHR;
  } catch (const vk::SystemError &e) {
    MC_CRITICAL("Failed to present swapchain image! Error: {}", e.what());
    throw std::runtime_error("failed to present swapchain image!");
  }

  if (result == vk::Result::eErrorOutOfDateKHR ||
      result == vk::Result::eSuboptimalKHR) {
    m_Context.GetSwapchain()->Recreate();
  } else if (result != vk::Result::eSuccess) {
    MC_CRITICAL("Failed to present swapchain image!");
    throw std::runtime_error("failed to present swapchain image!");
  }

  m_IsFrameStarted = false;
  m_CurrentFrameIndex = (m_CurrentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::BeginRenderPass(vk::CommandBuffer commandBuffer) {
  vk::RenderingAttachmentInfo colorAttachment;
  colorAttachment.imageView =
      m_Context.GetSwapchain()->GetImageViews()[m_CurrentImageIndex];
  colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
  colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
  colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
  colorAttachment.clearValue.color =
      vk::ClearColorValue(std::array<float, 4>{0.01f, 0.01f, 0.01f, 1.0f});

  vk::RenderingInfo renderingInfo;
  renderingInfo.renderArea.offset = vk::Offset2D{0, 0};
  renderingInfo.renderArea.extent = m_Context.GetSwapchain()->GetExtent();
  renderingInfo.layerCount = 1;
  renderingInfo.colorAttachmentCount = 1;
  renderingInfo.pColorAttachments = &colorAttachment;

  // Dynamic Rendering layout transitions - wait, we must transition the image
  // layout! BUT we also need to use dynamic rendering correctly. Oh, we need
  // image layout transitions. Actually, later in pipeline or frame logic we add
  // full image barriers. For now, let vk::CommandBuffer::beginRendering handle
  // the rendering info. Note: if there's no render pass, the developer is
  // expected to transition image to eColorAttachmentOptimal before rendering!

  vk::ImageMemoryBarrier barrier;
  barrier.oldLayout = vk::ImageLayout::eUndefined;
  barrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = m_Context.GetSwapchain()->GetImages()[m_CurrentImageIndex];
  barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = vk::AccessFlags{};
  barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

  commandBuffer.pipelineBarrier(
      vk::PipelineStageFlagBits::eTopOfPipe,
      vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::DependencyFlags{},
      nullptr, nullptr, barrier);

  commandBuffer.beginRendering(renderingInfo);

  // Set Viewport and Scissor for dynamic states
  vk::Viewport viewport;
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width =
      static_cast<float>(m_Context.GetSwapchain()->GetExtent().width);
  viewport.height =
      static_cast<float>(m_Context.GetSwapchain()->GetExtent().height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  commandBuffer.setViewport(0, 1, &viewport);

  vk::Rect2D scissor;
  scissor.offset = vk::Offset2D{0, 0};
  scissor.extent = m_Context.GetSwapchain()->GetExtent();
  commandBuffer.setScissor(0, 1, &scissor);
}

void VulkanRenderer::EndRenderPass(vk::CommandBuffer commandBuffer) {
  commandBuffer.endRendering();

  // Transition image back to PRESENT layout
  vk::ImageMemoryBarrier barrier;
  barrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
  barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = m_Context.GetSwapchain()->GetImages()[m_CurrentImageIndex];
  barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
  barrier.dstAccessMask = vk::AccessFlags{};

  commandBuffer.pipelineBarrier(
      vk::PipelineStageFlagBits::eColorAttachmentOutput,
      vk::PipelineStageFlagBits::eBottomOfPipe, vk::DependencyFlags{}, nullptr,
      nullptr, barrier);
}

void VulkanRenderer::Draw(vk::CommandBuffer commandBuffer,
                          VulkanPipeline &pipeline, vk::Buffer vertexBuffer,
                          vk::Buffer indexBuffer, uint32_t indexCount) {
  commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                             pipeline.GetPipeline());

  vk::Buffer vertexBuffers[] = {vertexBuffer};
  vk::DeviceSize offsets[] = {0};
  commandBuffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);
  commandBuffer.bindIndexBuffer(indexBuffer, 0, vk::IndexType::eUint32);

  commandBuffer.drawIndexed(indexCount, 1, 0, 0, 0);
}

void VulkanRenderer::DrawMesh(vk::CommandBuffer commandBuffer,
                              VulkanPipeline &pipeline, VulkanMesh &mesh) {
  commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                             pipeline.GetPipeline());
  mesh.Bind(commandBuffer);
  mesh.Draw(commandBuffer);
}

} // namespace mc
