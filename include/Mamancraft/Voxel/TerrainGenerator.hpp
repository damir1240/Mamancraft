#pragma once

#include "Mamancraft/Voxel/Biome.hpp"
#include "Mamancraft/Voxel/Chunk.hpp"

namespace mc {

/**
 * @brief Abstract base for terrain generation.
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
 * @brief Advanced Terrain Generator with biome support.
 *
 * Uses temperature/humidity noise for biome selection (Whittaker-style),
 * multi-layered noise for terrain shape, and procedural tree placement.
 * Transitions between biomes are smooth due to continuous noise.
 */
class AdvancedTerrainGenerator : public TerrainGenerator {
public:
  AdvancedTerrainGenerator(uint32_t seed = 1337);

  void Generate(Chunk &chunk) const override;

private:
  uint32_t m_Seed;

  // Biome selection based on temperature/humidity noise
  BiomeType GetBiome(float temperature, float humidity) const;

  // Height calculation (all biome-blended, preferred)
  float GetTerrainHeightBlended(float baseVal, float detailVal,
                                float mountainVal, float temperature,
                                float humidity) const;

  // Legacy single-biome height stub (kept for reference)
  float GetTerrainHeight(float baseVal, float detailVal, float mountainVal,
                         BiomeType biome) const;

  // Surface block based on biome and height
  BlockType GetSurfaceBlock(BiomeType biome, int worldY,
                            int terrainHeight) const;

  // Deterministic tree placement check
  bool ShouldPlaceTree(int worldX, int worldZ, BiomeType biome) const;

  // Cave generation placeholder
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
