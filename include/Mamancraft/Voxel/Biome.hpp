#pragma once
#include "Mamancraft/Voxel/Block.hpp"
#include <cstdint>

namespace mc {

enum class BiomeType : uint8_t { Plains = 0, OakForest, Mountain, Count };

struct BiomeInfo {
  BiomeType type;
  BlockType surfaceBlock;
  BlockType subSurfaceBlock;
  float treeDensity;        // Probability [0,1] of tree placement per column
  int stoneHeightThreshold; // Above this height, surface becomes stone
                            // (mountains)
};

// Best Practice: Whittaker-style biome classification using
// temperature/humidity
inline BiomeInfo GetBiomeInfo(BiomeType biome) {
  switch (biome) {
  case BiomeType::Plains:
    return {BiomeType::Plains, BlockType::Grass, BlockType::Dirt, 0.0f, 999};

  case BiomeType::OakForest:
    return {BiomeType::OakForest, BlockType::Grass, BlockType::Dirt, 0.06f,
            999};

  case BiomeType::Mountain:
    return {BiomeType::Mountain, BlockType::Grass, BlockType::Dirt, 0.0f, 80};

  default:
    return {BiomeType::Plains, BlockType::Grass, BlockType::Dirt, 0.0f, 999};
  }
}

} // namespace mc
