#pragma once

#include "Mamancraft/Core/TaskSystem.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanMesh.hpp"
#include "Mamancraft/Voxel/Chunk.hpp"
#include "Mamancraft/Voxel/TerrainGenerator.hpp"


#include <glm/gtx/hash.hpp>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>


namespace mc {

/**
 * @brief Manages a collection of chunks and world logic.
 * Best Practices:
 * - std::shared_mutex for thread-safe chunk access.
 * - std::unordered_map with glm::ivec3 keys.
 * - Asynchronous loading and meshing using TaskSystem.
 */
class World {
public:
  World(std::unique_ptr<TerrainGenerator> generator, TaskSystem &taskSystem);
  ~World() = default;

  /**
   * @brief Update world state (e.g., load/unload chunks based on position).
   */
  void Update(const glm::vec3 &playerPos);

  /**
   * @brief Get and clear any newly generated meshes that need GPU upload.
   * This must be called from the main thread.
   */
  std::vector<std::pair<glm::ivec3, VulkanMesh::Builder>> GetPendingMeshes();

  std::shared_ptr<Chunk> GetChunk(const glm::ivec3 &position);
  bool HasChunk(const glm::ivec3 &position) const;

private:
  void RequestChunk(const glm::ivec3 &position);

  std::unique_ptr<TerrainGenerator> m_Generator;
  TaskSystem &m_TaskSystem;

  std::unordered_map<glm::ivec3, std::shared_ptr<Chunk>> m_Chunks;
  mutable std::shared_mutex m_WorldMutex;

  // Pending results from worker threads
  std::vector<std::pair<glm::ivec3, VulkanMesh::Builder>> m_PendingMeshes;
  std::mutex m_PendingMutex;

  // Track which chunks are currently being processed to avoid duplicate tasks
  std::unordered_set<glm::ivec3> m_LoadingChunks;
  std::mutex m_LoadingMutex;
};

} // namespace mc
