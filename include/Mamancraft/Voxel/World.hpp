#pragma once

#include "Mamancraft/Voxel/Chunk.hpp"
#include "Mamancraft/Voxel/TerrainGenerator.hpp"
#include <glm/gtx/hash.hpp>
#include <memory>
#include <shared_mutex>
#include <unordered_map>


namespace mc {

/**
 * @brief Manages a collection of chunks and world logic.
 * Best Practices:
 * - std::shared_mutex for thread-safe chunk access.
 * - std::unordered_map with glm::ivec3 keys.
 */
class World {
public:
  World(std::unique_ptr<TerrainGenerator> generator);
  ~World() = default;

  /**
   * @brief Get or generate a chunk at world coordinates.
   */
  std::shared_ptr<Chunk> GetChunk(const glm::ivec3 &position);

  /**
   * @brief Check if a chunk exists at the given position.
   */
  bool HasChunk(const glm::ivec3 &position) const;

  /**
   * @brief Generate and store a new chunk.
   */
  void LoadChunk(const glm::ivec3 &position);

private:
  std::unique_ptr<TerrainGenerator> m_Generator;
  std::unordered_map<glm::ivec3, std::shared_ptr<Chunk>> m_Chunks;
  mutable std::shared_mutex m_WorldMutex;
};

} // namespace mc
