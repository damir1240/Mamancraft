#include "Mamancraft/Renderer/IndirectDrawSystem.hpp"
#include "Mamancraft/Core/Logger.hpp"
#include "Mamancraft/Renderer/VulkanContext.hpp"

#include <cstring>
#include <stdexcept>

namespace mc {

IndirectDrawSystem::IndirectDrawSystem(VulkanContext &context,
                                       uint32_t maxDraws,
                                       vk::DeviceSize maxVertexBytes,
                                       vk::DeviceSize maxIndexBytes)
    : m_Context(context), m_MaxDraws(maxDraws),
      m_MaxVertexBytes(maxVertexBytes), m_MaxIndexBytes(maxIndexBytes) {

  VmaAllocator allocator = m_Context.GetAllocator()->GetAllocator();

  // --- Mega Vertex Buffer (device-local) ---
  m_MegaVertexBuffer = std::make_unique<VulkanBuffer>(
      allocator, maxVertexBytes, 1,
      vk::BufferUsageFlagBits::eVertexBuffer |
          vk::BufferUsageFlagBits::eTransferDst,
      VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);

  // --- Mega Index Buffer (device-local) ---
  m_MegaIndexBuffer = std::make_unique<VulkanBuffer>(
      allocator, maxIndexBytes, 1,
      vk::BufferUsageFlagBits::eIndexBuffer |
          vk::BufferUsageFlagBits::eTransferDst,
      VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);

  // --- Draw Command SSBO (host-visible for CPU updates + indirect draw) ---
  vk::DeviceSize drawCmdSize = sizeof(DrawCommand) * maxDraws;
  m_DrawCommandBuffer = std::make_unique<VulkanBuffer>(
      allocator, drawCmdSize, 1,
      vk::BufferUsageFlagBits::eStorageBuffer |
          vk::BufferUsageFlagBits::eIndirectBuffer,
      VMA_MEMORY_USAGE_AUTO,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
          VMA_ALLOCATION_CREATE_MAPPED_BIT);
  m_DrawCommandBuffer->Map();

  // --- Object Data SSBO (host-visible for CPU updates) ---
  vk::DeviceSize objectDataSize = sizeof(ObjectData) * maxDraws;
  m_ObjectDataBuffer = std::make_unique<VulkanBuffer>(
      allocator, objectDataSize, 1, vk::BufferUsageFlagBits::eStorageBuffer,
      VMA_MEMORY_USAGE_AUTO,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
          VMA_ALLOCATION_CREATE_MAPPED_BIT);
  m_ObjectDataBuffer->Map();

  // --- CPU-side arrays ---
  m_DrawCommands.resize(maxDraws);
  m_ObjectDataArray.resize(maxDraws);
  m_Allocations.resize(maxDraws);

  MC_INFO("IndirectDrawSystem: Initialized (maxDraws={}, vertexBuf={}MB, "
          "indexBuf={}MB)",
          maxDraws, maxVertexBytes / (1024 * 1024),
          maxIndexBytes / (1024 * 1024));
}

uint32_t IndirectDrawSystem::AddChunk(const std::vector<Vertex> &vertices,
                                      const std::vector<uint32_t> &indices,
                                      const ObjectData &objectData) {

  std::lock_guard lock(m_Mutex);

  // Allocate a draw ID
  uint32_t drawID;
  if (!m_FreeDrawIDs.empty()) {
    drawID = m_FreeDrawIDs.back();
    m_FreeDrawIDs.pop_back();
  } else {
    drawID = m_NextDrawID++;
    if (drawID >= m_MaxDraws) {
      MC_ERROR("IndirectDrawSystem: Max draw count ({}) exceeded!", m_MaxDraws);
      throw std::runtime_error("IndirectDrawSystem: Max draws exceeded");
    }
  }

  // Check vertex/index capacity
  vk::DeviceSize vertexDataSize =
      static_cast<vk::DeviceSize>(vertices.size()) * sizeof(Vertex);
  vk::DeviceSize indexDataSize =
      static_cast<vk::DeviceSize>(indices.size()) * sizeof(uint32_t);

  vk::DeviceSize vertexEnd =
      static_cast<vk::DeviceSize>(m_CurrentVertexOffset) * sizeof(Vertex) +
      vertexDataSize;
  vk::DeviceSize indexEnd =
      static_cast<vk::DeviceSize>(m_CurrentIndexOffset) * sizeof(uint32_t) +
      indexDataSize;

  if (vertexEnd > m_MaxVertexBytes) {
    MC_ERROR("IndirectDrawSystem: Mega vertex buffer overflow! ({}/{} bytes)",
             vertexEnd, m_MaxVertexBytes);
    m_FreeDrawIDs.push_back(drawID);
    throw std::runtime_error("IndirectDrawSystem: Vertex buffer overflow");
  }
  if (indexEnd > m_MaxIndexBytes) {
    MC_ERROR("IndirectDrawSystem: Mega index buffer overflow! ({}/{} bytes)",
             indexEnd, m_MaxIndexBytes);
    m_FreeDrawIDs.push_back(drawID);
    throw std::runtime_error("IndirectDrawSystem: Index buffer overflow");
  }

  // Record allocation
  ChunkAllocation &alloc = m_Allocations[drawID];
  alloc.vertexOffset = m_CurrentVertexOffset;
  alloc.vertexCount = static_cast<uint32_t>(vertices.size());
  alloc.indexOffset = m_CurrentIndexOffset;
  alloc.indexCount = static_cast<uint32_t>(indices.size());
  alloc.active = true;

  // Upload vertex data via staging
  {
    VulkanBuffer stagingVB(
        m_Context.GetAllocator()->GetAllocator(), vertexDataSize, 1,
        vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    stagingVB.Map();
    stagingVB.WriteToBuffer(const_cast<Vertex *>(vertices.data()),
                            vertexDataSize);
    stagingVB.Unmap();

    m_Context.ImmediateSubmit([&](vk::CommandBuffer cmd) {
      vk::BufferCopy region;
      region.srcOffset = 0;
      region.dstOffset =
          static_cast<vk::DeviceSize>(m_CurrentVertexOffset) * sizeof(Vertex);
      region.size = vertexDataSize;
      cmd.copyBuffer(stagingVB.GetBuffer(), m_MegaVertexBuffer->GetBuffer(),
                     region);
    });
  }

  // Upload index data via staging
  {
    VulkanBuffer stagingIB(
        m_Context.GetAllocator()->GetAllocator(), indexDataSize, 1,
        vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    stagingIB.Map();
    stagingIB.WriteToBuffer(const_cast<uint32_t *>(indices.data()),
                            indexDataSize);
    stagingIB.Unmap();

    m_Context.ImmediateSubmit([&](vk::CommandBuffer cmd) {
      vk::BufferCopy region;
      region.srcOffset = 0;
      region.dstOffset =
          static_cast<vk::DeviceSize>(m_CurrentIndexOffset) * sizeof(uint32_t);
      region.size = indexDataSize;
      cmd.copyBuffer(stagingIB.GetBuffer(), m_MegaIndexBuffer->GetBuffer(),
                     region);
    });
  }

  // Fill draw command
  DrawCommand &cmd = m_DrawCommands[drawID];
  cmd.indexCount = alloc.indexCount;
  cmd.instanceCount = 1;
  cmd.firstIndex = alloc.indexOffset;
  cmd.vertexOffset = static_cast<int32_t>(alloc.vertexOffset);
  cmd.firstInstance = drawID; // gl_BaseInstance

  // Fill object data
  m_ObjectDataArray[drawID] = objectData;

  // Advance offsets
  m_CurrentVertexOffset += alloc.vertexCount;
  m_CurrentIndexOffset += alloc.indexCount;
  m_ActiveDrawCount++;

  m_DirtyDrawCommands = true;
  m_DirtyObjectData = true;

  return drawID;
}

void IndirectDrawSystem::RemoveChunk(uint32_t drawID) {
  std::lock_guard lock(m_Mutex);

  if (drawID >= m_MaxDraws || !m_Allocations[drawID].active) {
    MC_WARN("IndirectDrawSystem: RemoveChunk called with invalid drawID={}",
            drawID);
    return;
  }

  // Zero out the draw command (won't be drawn)
  m_DrawCommands[drawID].instanceCount = 0;
  m_DrawCommands[drawID].indexCount = 0;
  m_Allocations[drawID].active = false;

  // Return the drawID to the free list
  m_FreeDrawIDs.push_back(drawID);
  m_ActiveDrawCount--;

  m_DirtyDrawCommands = true;
}

void IndirectDrawSystem::UpdateObjectData(uint32_t drawID,
                                          const ObjectData &data) {
  std::lock_guard lock(m_Mutex);

  if (drawID >= m_MaxDraws || !m_Allocations[drawID].active) {
    MC_WARN(
        "IndirectDrawSystem: UpdateObjectData called with invalid drawID={}",
        drawID);
    return;
  }

  m_ObjectDataArray[drawID] = data;
  m_DirtyObjectData = true;
}

void IndirectDrawSystem::ResetDrawCommands() {
  std::lock_guard lock(m_Mutex);

  // Reset all active draw commands to visible (instanceCount = 1)
  // Compute culling shader will set instanceCount = 0 for invisible chunks
  for (uint32_t i = 0; i < m_NextDrawID; i++) {
    if (m_Allocations[i].active) {
      m_DrawCommands[i].instanceCount = 1;
    }
  }

  m_DirtyDrawCommands = true;
}

void IndirectDrawSystem::FlushToGPU() {
  std::lock_guard lock(m_Mutex);

  if (m_DirtyDrawCommands) {
    vk::DeviceSize size = sizeof(DrawCommand) * m_NextDrawID;
    if (size > 0) {
      m_DrawCommandBuffer->WriteToBuffer(m_DrawCommands.data(), size);
    }
    m_DirtyDrawCommands = false;
  }

  if (m_DirtyObjectData) {
    vk::DeviceSize size = sizeof(ObjectData) * m_NextDrawID;
    if (size > 0) {
      m_ObjectDataBuffer->WriteToBuffer(m_ObjectDataArray.data(), size);
    }
    m_DirtyObjectData = false;
  }
}

vk::Buffer IndirectDrawSystem::GetVertexBuffer() const {
  return m_MegaVertexBuffer->GetBuffer();
}

vk::Buffer IndirectDrawSystem::GetIndexBuffer() const {
  return m_MegaIndexBuffer->GetBuffer();
}

vk::Buffer IndirectDrawSystem::GetDrawCommandBuffer() const {
  return m_DrawCommandBuffer->GetBuffer();
}

vk::Buffer IndirectDrawSystem::GetObjectDataBuffer() const {
  return m_ObjectDataBuffer->GetBuffer();
}

vk::DeviceSize IndirectDrawSystem::GetDrawCommandBufferSize() const {
  return sizeof(DrawCommand) * m_MaxDraws;
}

vk::DeviceSize IndirectDrawSystem::GetObjectDataBufferSize() const {
  return sizeof(ObjectData) * m_MaxDraws;
}

} // namespace mc
