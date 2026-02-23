#pragma once

#include "Mamancraft/Voxel/Chunk.hpp"

namespace mc {

/**
 * @brief Abstract base for terrain generation.
 * Follows C++20 best practices (stateless where possible).
 */
class TerrainGenerator {
public:
  virtual ~TerrainGenerator() = default;

  /**
   * @brief Populate chunk data with blocks.
   */
  virtual void Generate(Chunk &chunk) const = 0;
};

/**
 * @brief Simple Wave Generator (Legacy Logic)
 */
class WaveTerrainGenerator : public TerrainGenerator {
public:
  void Generate(Chunk &chunk) const override;
};

} // namespace mc
