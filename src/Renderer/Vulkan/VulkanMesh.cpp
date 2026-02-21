#include "Mamancraft/Renderer/Vulkan/VulkanMesh.hpp"
#include "Mamancraft/Renderer/VulkanContext.hpp"
#include <cassert>

namespace mc {

VulkanMesh::VulkanMesh(VulkanContext &context, const Builder &builder)
    : m_Context(context) {
  CreateVertexBuffers(builder.vertices);
  CreateIndexBuffers(builder.indices);
}

VulkanMesh::~VulkanMesh() {}

void VulkanMesh::CreateVertexBuffers(const std::vector<Vertex> &vertices) {
  m_VertexCount = static_cast<uint32_t>(vertices.size());
  assert(m_VertexCount >= 3 && "Vertex count must be at least 3");
  vk::DeviceSize bufferSize = sizeof(vertices[0]) * m_VertexCount;

  VulkanBuffer stagingBuffer(
      m_Context.GetAllocator()->GetAllocator(), bufferSize, 1,
      vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_CPU_ONLY);

  stagingBuffer.Map();
  stagingBuffer.WriteToBuffer((void *)vertices.data(), bufferSize);
  stagingBuffer.Unmap();

  m_VertexBuffer = std::make_unique<VulkanBuffer>(
      m_Context.GetAllocator()->GetAllocator(), bufferSize, 1,
      vk::BufferUsageFlagBits::eVertexBuffer |
          vk::BufferUsageFlagBits::eTransferDst,
      VMA_MEMORY_USAGE_GPU_ONLY);

  VulkanBuffer::CopyBuffer(m_Context, stagingBuffer.GetBuffer(),
                           m_VertexBuffer->GetBuffer(), bufferSize);
}

void VulkanMesh::CreateIndexBuffers(const std::vector<uint32_t> &indices) {
  m_IndexCount = static_cast<uint32_t>(indices.size());
  m_HasIndexBuffer = m_IndexCount > 0;

  if (!m_HasIndexBuffer) {
    return;
  }

  vk::DeviceSize bufferSize = sizeof(indices[0]) * m_IndexCount;

  VulkanBuffer stagingBuffer(
      m_Context.GetAllocator()->GetAllocator(), bufferSize, 1,
      vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_CPU_ONLY);

  stagingBuffer.Map();
  stagingBuffer.WriteToBuffer((void *)indices.data(), bufferSize);
  stagingBuffer.Unmap();

  m_IndexBuffer = std::make_unique<VulkanBuffer>(
      m_Context.GetAllocator()->GetAllocator(), bufferSize, 1,
      vk::BufferUsageFlagBits::eIndexBuffer |
          vk::BufferUsageFlagBits::eTransferDst,
      VMA_MEMORY_USAGE_GPU_ONLY);

  VulkanBuffer::CopyBuffer(m_Context, stagingBuffer.GetBuffer(),
                           m_IndexBuffer->GetBuffer(), bufferSize);
}

void VulkanMesh::Bind(vk::CommandBuffer commandBuffer) {
  vk::Buffer buffers[] = {m_VertexBuffer->GetBuffer()};
  vk::DeviceSize offsets[] = {0};
  commandBuffer.bindVertexBuffers(0, 1, buffers, offsets);

  if (m_HasIndexBuffer) {
    commandBuffer.bindIndexBuffer(m_IndexBuffer->GetBuffer(), 0,
                                  vk::IndexType::eUint32);
  }
}

void VulkanMesh::Draw(vk::CommandBuffer commandBuffer) {
  if (m_HasIndexBuffer) {
    commandBuffer.drawIndexed(m_IndexCount, 1, 0, 0, 0);
  } else {
    commandBuffer.draw(m_VertexCount, 1, 0, 0);
  }
}

} // namespace mc
