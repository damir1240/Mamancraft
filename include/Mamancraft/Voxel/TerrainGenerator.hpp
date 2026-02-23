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
 * @brief Advanced Terrain Generator using FastNoiseLite.
 * Implements multi-layered noise for majestic terrains.
 */
class AdvancedTerrainGenerator : public TerrainGenerator {
public:
  AdvancedTerrainGenerator(uint32_t seed = 1337);

  void Generate(Chunk &chunk) const override;

private:
  uint32_t m_Seed;

  // Internal methods for features (placeholders for now)
  float GetHeight(float x, float z) const;
  BlockType GetBiomeAt(float x, float z) const;
  bool HasCaveAt(float x, float y, float z) const;
};

/**
 * @brief Simple Wave Generator (Legacy Logic)
 */
class WaveTerrainGenerator : public TerrainGenerator {
public:
  void Generate(Chunk &chunk) const override;
};

} // namespace mc
