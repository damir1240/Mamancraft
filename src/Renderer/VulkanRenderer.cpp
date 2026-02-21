#include "Mamancraft/Renderer/VulkanRenderer.hpp"
#include "Mamancraft/Core/Logger.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanCommandPool.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanMesh.hpp"

#include <stdexcept>

namespace mc {

VulkanRenderer::VulkanRenderer(VulkanContext &context) : m_Context(context) {
  CreateCommandBuffers();
  CreateSyncObjects();
  CreateDescriptors();
  CreateUboBuffers();
}

VulkanRenderer::~VulkanRenderer() {
  MC_DEBUG("VulkanRenderer destructor: Starting cleanup");

  vk::Device device = m_Context.GetDevice()->GetLogicalDevice();

  device.waitIdle();

  m_CommandBuffers.clear();
  m_UboBuffers.clear();

  if (m_DescriptorPool) {
    device.destroyDescriptorPool(m_DescriptorPool);
  }
  if (m_GlobalDescriptorSetLayout) {
    device.destroyDescriptorSetLayout(m_GlobalDescriptorSetLayout);
  }

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

void VulkanRenderer::CreateDescriptors() {
  vk::DescriptorSetLayoutBinding uboLayoutBinding{};
  uboLayoutBinding.binding = 0;
  uboLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
  uboLayoutBinding.descriptorCount = 1;
  uboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;

  vk::DescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.bindingCount = 1;
  layoutInfo.pBindings = &uboLayoutBinding;

  m_GlobalDescriptorSetLayout =
      m_Context.GetDevice()->GetLogicalDevice().createDescriptorSetLayout(
          layoutInfo);

  std::vector<vk::DescriptorPoolSize> poolSizes = {
      {vk::DescriptorType::eUniformBuffer,
       static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)}};

  vk::DescriptorPoolCreateInfo poolInfo{};
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

  m_DescriptorPool =
      m_Context.GetDevice()->GetLogicalDevice().createDescriptorPool(poolInfo);
}

void VulkanRenderer::CreateUboBuffers() {
  m_UboBuffers.resize(MAX_FRAMES_IN_FLIGHT);
  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    m_UboBuffers[i] = std::make_unique<VulkanBuffer>(
        m_Context.GetAllocator()->GetAllocator(), sizeof(GlobalUbo), 1,
        vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT,
        m_Context.GetDevice()
            ->GetPhysicalDevice()
            .getProperties()
            .limits.minUniformBufferOffsetAlignment);
    m_UboBuffers[i]->Map();
  }

  std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT,
                                               m_GlobalDescriptorSetLayout);
  vk::DescriptorSetAllocateInfo allocInfo{};
  allocInfo.descriptorPool = m_DescriptorPool;
  allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
  allocInfo.pSetLayouts = layouts.data();

  // Handle Vulkan-HPP result value or direct vector
  m_GlobalDescriptorSets =
      m_Context.GetDevice()->GetLogicalDevice().allocateDescriptorSets(
          allocInfo);

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vk::DescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = m_UboBuffers[i]->GetBuffer();
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(GlobalUbo);

    vk::WriteDescriptorSet descriptorWrite{};
    descriptorWrite.dstSet = m_GlobalDescriptorSets[i];
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    m_Context.GetDevice()->GetLogicalDevice().updateDescriptorSets(
        1, &descriptorWrite, 0, nullptr);
  }
}

void VulkanRenderer::UpdateGlobalUbo(const GlobalUbo &ubo) {
  m_UboBuffers[m_CurrentFrameIndex]->WriteToBuffer((void *)&ubo);
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
    m_Context.GetSwapchain()->Recreate();
    return nullptr;
  } else if (result != vk::Result::eSuccess &&
             result != vk::Result::eSuboptimalKHR) {
    MC_CRITICAL("Failed to acquire swapchain image!");
    throw std::runtime_error("failed to acquire swapchain image!");
  }

  (void)device.resetFences(1, &m_InFlightFences[m_CurrentFrameIndex]);

  auto commandBuffer =
      m_CommandBuffers[m_CurrentFrameIndex]->GetCommandBuffer();

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

  vk::RenderingAttachmentInfo depthAttachment;
  depthAttachment.imageView = m_Context.GetSwapchain()->GetDepthImageView();
  depthAttachment.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
  depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
  depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
  depthAttachment.clearValue.depthStencil = vk::ClearDepthStencilValue{1.0f, 0};

  vk::RenderingInfo renderingInfo;
  renderingInfo.renderArea.offset = vk::Offset2D{0, 0};
  renderingInfo.renderArea.extent = m_Context.GetSwapchain()->GetExtent();
  renderingInfo.layerCount = 1;
  renderingInfo.colorAttachmentCount = 1;
  renderingInfo.pColorAttachments = &colorAttachment;
  renderingInfo.pDepthAttachment = &depthAttachment;

  // Image layout transitions
  vk::ImageMemoryBarrier colorBarrier;
  colorBarrier.oldLayout = vk::ImageLayout::eUndefined;
  colorBarrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
  colorBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  colorBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  colorBarrier.image =
      m_Context.GetSwapchain()->GetImages()[m_CurrentImageIndex];
  colorBarrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
  colorBarrier.subresourceRange.baseMipLevel = 0;
  colorBarrier.subresourceRange.levelCount = 1;
  colorBarrier.subresourceRange.baseArrayLayer = 0;
  colorBarrier.subresourceRange.layerCount = 1;
  colorBarrier.srcAccessMask = vk::AccessFlags{};
  colorBarrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

  vk::ImageMemoryBarrier depthBarrier;
  depthBarrier.oldLayout = vk::ImageLayout::eUndefined;
  depthBarrier.newLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
  depthBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  depthBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  depthBarrier.image = m_Context.GetSwapchain()->GetDepthImage();
  depthBarrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
  depthBarrier.subresourceRange.baseMipLevel = 0;
  depthBarrier.subresourceRange.levelCount = 1;
  depthBarrier.subresourceRange.baseArrayLayer = 0;
  depthBarrier.subresourceRange.layerCount = 1;
  depthBarrier.srcAccessMask = vk::AccessFlags{};
  depthBarrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;

  vk::ImageMemoryBarrier barriers[] = {colorBarrier, depthBarrier};

  commandBuffer.pipelineBarrier(
      vk::PipelineStageFlagBits::eTopOfPipe,
      vk::PipelineStageFlagBits::eColorAttachmentOutput |
          vk::PipelineStageFlagBits::eEarlyFragmentTests,
      vk::DependencyFlags{}, nullptr, nullptr, barriers);

  commandBuffer.beginRendering(renderingInfo);

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

void VulkanRenderer::DrawMesh(vk::CommandBuffer commandBuffer,
                              VulkanPipeline &pipeline, VulkanMesh &mesh,
                              const PushConstantData &pushData) {
  commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                             pipeline.GetPipeline());

  commandBuffer.bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics, pipeline.GetPipelineLayout(), 0, 1,
      &m_GlobalDescriptorSets[m_CurrentFrameIndex], 0, nullptr);

  commandBuffer.pushConstants(pipeline.GetPipelineLayout(),
                              vk::ShaderStageFlagBits::eVertex, 0,
                              sizeof(PushConstantData), &pushData);

  mesh.Bind(commandBuffer);
  mesh.Draw(commandBuffer);
}

} // namespace mc
