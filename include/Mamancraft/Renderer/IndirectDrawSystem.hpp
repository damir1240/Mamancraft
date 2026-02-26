#pragma once

#include "Mamancraft/Renderer/GPUStructures.hpp"
#include "Mamancraft/Renderer/Vertex.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanBuffer.hpp"
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace mc {

class VulkanContext;

/// GPU-Driven Indirect Draw System.
///
/// Manages a "mega buffer" architecture where ALL chunk geometry lives in
/// a single vertex buffer and a single index buffer. Each chunk gets a
/// slot (drawID) with its own DrawCommand and ObjectData entry.
///
/// Rendering uses a single vkCmdDrawIndexedIndirect call that processes
/// all visible chunks. Visibility is determined by a compute shader that
/// sets instanceCount to 0 for culled chunks.
///
/// Best practices implemented:
///   - Free-list allocator for draw slot reuse
///   - Device-local mega buffers with staging upload
///   - Atomic draw count tracking
class IndirectDrawSystem {
public:
  IndirectDrawSystem(VulkanContext &context, uint32_t maxDraws,
                     vk::DeviceSize maxVertexBytes,
                     vk::DeviceSize maxIndexBytes);
  ~IndirectDrawSystem() = default;

  IndirectDrawSystem(const IndirectDrawSystem &) = delete;
  IndirectDrawSystem &operator=(const IndirectDrawSystem &) = delete;

  /// Add a chunk's geometry to the mega buffer.
  /// Returns a drawID that can be used for RemoveChunk/UpdateObjectData.
  /// Thread-safe: may be called from multiple threads.
  uint32_t AddChunk(const std::vector<Vertex> &vertices,
                    const std::vector<uint32_t> &indices,
                    const ObjectData &objectData);

  /// Remove a chunk from the system, freeing its draw slot.
  /// The vertex/index memory is NOT freed (fragmentation is acceptable
  /// for a voxel game where chunks are loaded/unloaded continuously).
  void RemoveChunk(uint32_t drawID);

  /// Update object data for an existing draw slot (e.g., after model matrix
  /// change).
  void UpdateObjectData(uint32_t drawID, const ObjectData &data);

  /// Reset all draw commands for this frame (set instanceCount = 1).
  /// Called once per frame before compute culling.
  void ResetDrawCommands();

  /// Upload modified CPU-side data to GPU buffers.
  /// Call once per frame after all AddChunk/UpdateObjectData calls.
  void FlushToGPU();

  // --- Accessors for binding ---
  [[nodiscard]] vk::Buffer GetVertexBuffer() const;
  [[nodiscard]] vk::Buffer GetIndexBuffer() const;
  [[nodiscard]] vk::Buffer GetDrawCommandBuffer() const;
  [[nodiscard]] vk::Buffer GetObjectDataBuffer() const;
  [[nodiscard]] uint32_t GetActiveDrawCount() const {
    return m_ActiveDrawCount;
  }
  [[nodiscard]] vk::DeviceSize GetDrawCommandBufferSize() const;
  [[nodiscard]] vk::DeviceSize GetObjectDataBufferSize() const;

private:
  struct ChunkAllocation {
    uint32_t vertexOffset = 0;
    uint32_t vertexCount = 0;
    uint32_t indexOffset = 0;
    uint32_t indexCount = 0;
    bool active = false;
  };

  VulkanContext &m_Context;
  uint32_t m_MaxDraws;

  // --- Mega Buffers (device-local) ---
  std::unique_ptr<VulkanBuffer> m_MegaVertexBuffer;
  std::unique_ptr<VulkanBuffer> m_MegaIndexBuffer;

  // --- SSBO Buffers (host-visible for simplicity, upgraded to device-local
  // later) ---
  std::unique_ptr<VulkanBuffer> m_DrawCommandBuffer;
  std::unique_ptr<VulkanBuffer> m_ObjectDataBuffer;

  // --- CPU-side mirrors ---
  std::vector<DrawCommand> m_DrawCommands;
  std::vector<ObjectData> m_ObjectDataArray;
  std::vector<ChunkAllocation> m_Allocations;

  // --- Free-list for draw slot reuse ---
  std::vector<uint32_t> m_FreeDrawIDs;
  uint32_t m_NextDrawID = 0;
  uint32_t m_ActiveDrawCount = 0;

  // --- Vertex/Index allocation tracking ---
  uint32_t m_CurrentVertexOffset = 0;
  uint32_t m_CurrentIndexOffset = 0;
  vk::DeviceSize m_MaxVertexBytes;
  vk::DeviceSize m_MaxIndexBytes;

  // Thread safety
  std::mutex m_Mutex;

  bool m_DirtyDrawCommands = false;
  bool m_DirtyObjectData = false;
};

} // namespace mc
