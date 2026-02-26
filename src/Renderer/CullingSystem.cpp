#include "Mamancraft/Renderer/CullingSystem.hpp"
#include "Mamancraft/Core/FileSystem.hpp"
#include "Mamancraft/Core/Logger.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanShader.hpp"
#include "Mamancraft/Renderer/VulkanContext.hpp"

#include <stdexcept>

namespace mc {

CullingSystem::CullingSystem(VulkanContext &context) : m_Context(context) {
  CreateDescriptorSetLayout();
  CreateDescriptorPool();
  CreatePipeline();

  // Create uniform buffer for CullUniforms (host-visible, mapped)
  m_UniformBuffer = std::make_unique<VulkanBuffer>(
      m_Context.GetAllocator()->GetAllocator(), sizeof(CullUniforms), 1,
      vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_AUTO,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
          VMA_ALLOCATION_CREATE_MAPPED_BIT);
  m_UniformBuffer->Map();

  MC_INFO("CullingSystem: Initialized compute frustum culling pipeline");
}

CullingSystem::~CullingSystem() {
  vk::Device device = m_Context.GetDevice()->GetLogicalDevice();
  device.waitIdle();

  if (m_Pipeline) {
    device.destroyPipeline(m_Pipeline);
  }
  if (m_PipelineLayout) {
    device.destroyPipelineLayout(m_PipelineLayout);
  }
  if (m_DescriptorPool) {
    device.destroyDescriptorPool(m_DescriptorPool);
  }
  if (m_DescriptorSetLayout) {
    device.destroyDescriptorSetLayout(m_DescriptorSetLayout);
  }
}

void CullingSystem::CreateDescriptorSetLayout() {
  vk::Device device = m_Context.GetDevice()->GetLogicalDevice();

  // Binding 0: CullUniforms (UBO)
  // Binding 1: DrawCommands (SSBO, read-write)
  // Binding 2: ObjectData (SSBO, read-only)
  std::array<vk::DescriptorSetLayoutBinding, 3> bindings{};

  bindings[0].binding = 0;
  bindings[0].descriptorType = vk::DescriptorType::eUniformBuffer;
  bindings[0].descriptorCount = 1;
  bindings[0].stageFlags = vk::ShaderStageFlagBits::eCompute;

  bindings[1].binding = 1;
  bindings[1].descriptorType = vk::DescriptorType::eStorageBuffer;
  bindings[1].descriptorCount = 1;
  bindings[1].stageFlags = vk::ShaderStageFlagBits::eCompute;

  bindings[2].binding = 2;
  bindings[2].descriptorType = vk::DescriptorType::eStorageBuffer;
  bindings[2].descriptorCount = 1;
  bindings[2].stageFlags = vk::ShaderStageFlagBits::eCompute;

  vk::DescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
  layoutInfo.pBindings = bindings.data();

  m_DescriptorSetLayout = device.createDescriptorSetLayout(layoutInfo);
}

void CullingSystem::CreateDescriptorPool() {
  vk::Device device = m_Context.GetDevice()->GetLogicalDevice();

  std::array<vk::DescriptorPoolSize, 2> poolSizes{};
  poolSizes[0].type = vk::DescriptorType::eUniformBuffer;
  poolSizes[0].descriptorCount = 1;
  poolSizes[1].type = vk::DescriptorType::eStorageBuffer;
  poolSizes[1].descriptorCount = 2;

  vk::DescriptorPoolCreateInfo poolInfo{};
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  poolInfo.maxSets = 1;

  m_DescriptorPool = device.createDescriptorPool(poolInfo);

  // Allocate descriptor set
  vk::DescriptorSetAllocateInfo allocInfo{};
  allocInfo.descriptorPool = m_DescriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &m_DescriptorSetLayout;

  m_DescriptorSet = device.allocateDescriptorSets(allocInfo)[0];
}

void CullingSystem::CreatePipeline() {
  vk::Device device = m_Context.GetDevice()->GetLogicalDevice();

  // Load compute shader
  std::string shaderPath =
      (FileSystem::GetExecutableDir() / "assets" / "base" / "assets" / "mc" /
       "shaders" / "frustum_cull.comp.spv")
          .string();

  VulkanShader computeShader(m_Context.GetDevice(), shaderPath);

  // Pipeline layout
  vk::PipelineLayoutCreateInfo layoutInfo{};
  layoutInfo.setLayoutCount = 1;
  layoutInfo.pSetLayouts = &m_DescriptorSetLayout;

  m_PipelineLayout = device.createPipelineLayout(layoutInfo);

  // Compute pipeline
  vk::PipelineShaderStageCreateInfo shaderStage{};
  shaderStage.stage = vk::ShaderStageFlagBits::eCompute;
  shaderStage.module = computeShader.GetShaderModule();
  shaderStage.pName = "main";

  vk::ComputePipelineCreateInfo pipelineInfo{};
  pipelineInfo.stage = shaderStage;
  pipelineInfo.layout = m_PipelineLayout;

  auto result = device.createComputePipeline(nullptr, pipelineInfo);
  if (result.result != vk::Result::eSuccess) {
    MC_CRITICAL("CullingSystem: Failed to create compute pipeline!");
    throw std::runtime_error("Failed to create frustum culling pipeline");
  }
  m_Pipeline = result.value;
}

void CullingSystem::Execute(vk::CommandBuffer commandBuffer,
                            const CullUniforms &uniforms,
                            vk::Buffer drawCommandBuffer,
                            vk::DeviceSize drawCommandBufferSize,
                            vk::Buffer objectDataBuffer,
                            vk::DeviceSize objectDataBufferSize,
                            uint32_t drawCount) {
  if (drawCount == 0) {
    return;
  }

  // Update uniform buffer
  m_UniformBuffer->WriteToBuffer(const_cast<CullUniforms *>(&uniforms),
                                 sizeof(CullUniforms));

  // Update descriptor set with current buffers
  vk::DescriptorBufferInfo uniformInfo{};
  uniformInfo.buffer = m_UniformBuffer->GetBuffer();
  uniformInfo.offset = 0;
  uniformInfo.range = sizeof(CullUniforms);

  vk::DescriptorBufferInfo drawCmdInfo{};
  drawCmdInfo.buffer = drawCommandBuffer;
  drawCmdInfo.offset = 0;
  drawCmdInfo.range = drawCommandBufferSize;

  vk::DescriptorBufferInfo objectDataInfo{};
  objectDataInfo.buffer = objectDataBuffer;
  objectDataInfo.offset = 0;
  objectDataInfo.range = objectDataBufferSize;

  std::array<vk::WriteDescriptorSet, 3> writes{};

  writes[0].dstSet = m_DescriptorSet;
  writes[0].dstBinding = 0;
  writes[0].descriptorType = vk::DescriptorType::eUniformBuffer;
  writes[0].descriptorCount = 1;
  writes[0].pBufferInfo = &uniformInfo;

  writes[1].dstSet = m_DescriptorSet;
  writes[1].dstBinding = 1;
  writes[1].descriptorType = vk::DescriptorType::eStorageBuffer;
  writes[1].descriptorCount = 1;
  writes[1].pBufferInfo = &drawCmdInfo;

  writes[2].dstSet = m_DescriptorSet;
  writes[2].dstBinding = 2;
  writes[2].descriptorType = vk::DescriptorType::eStorageBuffer;
  writes[2].descriptorCount = 1;
  writes[2].pBufferInfo = &objectDataInfo;

  m_Context.GetDevice()->GetLogicalDevice().updateDescriptorSets(writes,
                                                                 nullptr);

  // Bind compute pipeline and dispatch
  commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, m_Pipeline);
  commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                   m_PipelineLayout, 0, 1, &m_DescriptorSet, 0,
                                   nullptr);

  uint32_t workGroupCount = (drawCount + 63) / 64;
  commandBuffer.dispatch(workGroupCount, 1, 1);

  // Memory barrier: compute shader writes → indirect draw reads
  vk::MemoryBarrier2 barrier{};
  barrier.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader;
  barrier.srcAccessMask = vk::AccessFlagBits2::eShaderWrite;
  barrier.dstStageMask = vk::PipelineStageFlagBits2::eDrawIndirect;
  barrier.dstAccessMask = vk::AccessFlagBits2::eIndirectCommandRead;

  vk::DependencyInfo depInfo{};
  depInfo.memoryBarrierCount = 1;
  depInfo.pMemoryBarriers = &barrier;

  commandBuffer.pipelineBarrier2(depInfo);
}

} // namespace mc
