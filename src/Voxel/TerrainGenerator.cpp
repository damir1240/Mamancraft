#include "Mamancraft/Voxel/TerrainGenerator.hpp"
#include "FastNoiseLite.h"
#include <algorithm>
#include <cmath>

namespace mc {

AdvancedTerrainGenerator::AdvancedTerrainGenerator(uint32_t seed)
    : m_Seed(seed) {}

void AdvancedTerrainGenerator::Generate(Chunk &chunk) const {
  const glm::ivec3 chunkPos = chunk.GetPosition();

  // Minecraft-style constants
  constexpr int SEA_LEVEL = 62;
  constexpr int BASE_HEIGHT = 64;
  constexpr int HILL_AMPLITUDE = 10;  // gentle hills ±10 blocks (mostly flat)
  constexpr int DETAIL_AMPLITUDE = 3; // small bumps ±3 blocks
  constexpr int MOUNTAIN_AMPLITUDE = 30; // mountains up to +30 (rare)
  constexpr int DIRT_DEPTH = 4;
  constexpr int BEDROCK_MAX = 5;

  // Precompute absolute min/max possible terrain heights
  constexpr int MIN_HEIGHT =
      BASE_HEIGHT - HILL_AMPLITUDE - DETAIL_AMPLITUDE; // 51
  constexpr int MAX_HEIGHT = BASE_HEIGHT + HILL_AMPLITUDE + DETAIL_AMPLITUDE +
                             MOUNTAIN_AMPLITUDE; // 107

  int chunkBottomY = chunkPos.y * Chunk::SIZE;
  int chunkTopY = chunkBottomY + Chunk::SIZE - 1;

  // EARLY EXIT: Chunk entirely above max terrain → all air (already default)
  if (chunkBottomY > MAX_HEIGHT) {
    return; // Chunk constructor fills with Air
  }

  // EARLY EXIT: Chunk entirely below min terrain → all stone + bedrock
  if (chunkTopY < MIN_HEIGHT) {
    for (int x = 0; x < Chunk::SIZE; x++) {
      for (int z = 0; z < Chunk::SIZE; z++) {
        for (int y = 0; y < Chunk::SIZE; y++) {
          int worldY = chunkBottomY + y;
          chunk.SetBlock(
              x, y, z,
              {worldY < BEDROCK_MAX ? BlockType::Bedrock : BlockType::Stone});
        }
      }
    }
    return;
  }

  // --- Noise Configuration (Minecraft-style) ---
  // Base terrain shape: gentle rolling hills
  FastNoiseLite baseNoise;
  baseNoise.SetSeed(m_Seed);
  baseNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
  baseNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
  baseNoise.SetFractalOctaves(4);
  baseNoise.SetFractalLacunarity(2.0f);
  baseNoise.SetFractalGain(0.5f);
  baseNoise.SetFrequency(0.003f); // Broad gentle landscape

  // Detail noise: small bumps and texture on the terrain
  FastNoiseLite detailNoise;
  detailNoise.SetSeed(m_Seed + 100);
  detailNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
  detailNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
  detailNoise.SetFractalOctaves(2);
  detailNoise.SetFrequency(0.02f); // Fine details

  // Mountain noise: rare dramatic peaks
  FastNoiseLite mountainNoise;
  mountainNoise.SetSeed(m_Seed + 200);
  mountainNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
  mountainNoise.SetFrequency(
      0.003f); // Very large-scale → rare mountain regions

  for (int x = 0; x < Chunk::SIZE; x++) {
    for (int z = 0; z < Chunk::SIZE; z++) {
      float worldX = static_cast<float>(chunkPos.x * Chunk::SIZE + x);
      float worldZ = static_cast<float>(chunkPos.z * Chunk::SIZE + z);

      // 1. Base terrain height (smooth rolling hills)
      float baseVal = baseNoise.GetNoise(worldX, worldZ);     // [-1, 1]
      float detailVal = detailNoise.GetNoise(worldX, worldZ); // [-1, 1]

      // 2. Mountain factor (only rare peaks above 0.5 threshold)
      float mountainVal = mountainNoise.GetNoise(worldX, worldZ);
      float mountainFactor =
          std::max(0.0f, mountainVal - 0.5f) / 0.5f; // [0, 1], triggers rarely
      mountainFactor *= mountainFactor;              // square for sharper peaks

      // 3. Final height
      float height = BASE_HEIGHT + baseVal * HILL_AMPLITUDE +
                     detailVal * DETAIL_AMPLITUDE +
                     mountainFactor * MOUNTAIN_AMPLITUDE;

      int iHeight = static_cast<int>(height);

      // 4. Determine surface block type
      // TODO: Use biome noise to choose between grass/sand/snow etc.
      BlockType surfaceBlock = GetBiomeAt(worldX, worldZ);

      // 5. Fill the column
      for (int y = 0; y < Chunk::SIZE; y++) {
        int worldY = chunkPos.y * Chunk::SIZE + y;

        if (worldY > iHeight) {
          // Air above terrain
          chunk.SetBlock(x, y, z, {BlockType::Air});
        } else {
          // Cave check (placeholder)
          if (HasCaveAt(worldX, static_cast<float>(worldY), worldZ)) {
            chunk.SetBlock(x, y, z, {BlockType::Air});
            continue;
          }

          BlockType type;
          if (worldY < BEDROCK_MAX) {
            type = BlockType::Bedrock;
          } else if (worldY == iHeight) {
            type = surfaceBlock; // Grass/Sand on top
          } else if (worldY > iHeight - DIRT_DEPTH) {
            type = BlockType::Dirt; // Dirt layer under grass
          } else {
            type = BlockType::Stone; // Stone below everything
          }

          chunk.SetBlock(x, y, z, {type});
        }
      }
    }
  }
}

// TODO: Implement biome noise (Temperature/Humidity based biome selection)
float AdvancedTerrainGenerator::GetHeight(float x, float z) const {
  return 0.0f;
}

BlockType AdvancedTerrainGenerator::GetBiomeAt(float x, float z) const {
  // Placeholder: always grass. Future: use temperature/humidity noise.
  return BlockType::Grass;
}

bool AdvancedTerrainGenerator::HasCaveAt(float x, float y, float z) const {
  // TODO: 3D noise carving for caves (e.g., cheese/spaghetti caves)
  return false;
}

void WaveTerrainGenerator::Generate(Chunk &chunk) const {
  const glm::ivec3 chunkPos = chunk.GetPosition();

  for (int x = 0; x < Chunk::SIZE; x++) {
    for (int z = 0; z < Chunk::SIZE; z++) {
      float worldX = static_cast<float>(chunkPos.x * Chunk::SIZE + x);
      float worldZ = static_cast<float>(chunkPos.z * Chunk::SIZE + z);

      int height = static_cast<int>(32.0f + 10.0f * std::sin(worldX * 0.1f) *
                                                std::cos(worldZ * 0.1f));
      int worldYBase = chunkPos.y * Chunk::SIZE;

      for (int y = 0; y < Chunk::SIZE; y++) {
        int worldY = worldYBase + y;

        if (worldY < height) {
          BlockType type = BlockType::Stone;
          if (worldY == height - 1)
            type = BlockType::Grass;
          else if (worldY > height - 4)
            type = BlockType::Dirt;

          chunk.SetBlock(x, y, z, {type});
        } else {
          chunk.SetBlock(x, y, z, {BlockType::Air});
        }
      }
    }
  }
}

} // namespace mc
