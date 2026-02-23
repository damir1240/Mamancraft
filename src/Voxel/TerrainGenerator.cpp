#include "Mamancraft/Voxel/TerrainGenerator.hpp"
#include "FastNoiseLite.h"
#include "Mamancraft/Voxel/OakTreeGenerator.hpp"
#include <algorithm>
#include <cmath>

namespace mc {

AdvancedTerrainGenerator::AdvancedTerrainGenerator(uint32_t seed)
    : m_Seed(seed) {}

// ============================================================================
// Biome Selection (Whittaker-style temperature/humidity classification)
// ============================================================================

BiomeType AdvancedTerrainGenerator::GetBiome(float temperature,
                                             float humidity) const {
  // Classification: balanced distribution
  // OakForest is common but not overwhelming
  // Plains and Mountains get more space
  //
  // temperature controls "elevation tendency"
  // humidity controls "vegetation density"

  // Mountains: moderate threshold → noticeable but not dominant
  if (temperature > 0.35f) {
    return BiomeType::Mountain;
  }
  // Plains: when temperature OR humidity is low → decent coverage
  if (temperature < -0.2f || humidity < -0.3f) {
    return BiomeType::Plains;
  }
  // Forest: moderate temperature + decent humidity
  return BiomeType::OakForest;
}

// ============================================================================
// Terrain Height (per-biome shaping)
// ============================================================================

float AdvancedTerrainGenerator::GetTerrainHeight(float baseVal, float detailVal,
                                                 float mountainVal,
                                                 BiomeType biome) const {
  constexpr float BASE_HEIGHT = 64.0f;

  switch (biome) {
  case BiomeType::Plains:
    // Very flat: gentle hills ±6, subtle detail ±2
    return BASE_HEIGHT + baseVal * 6.0f + detailVal * 2.0f;

  case BiomeType::OakForest:
    // Moderate hills: ±10, more detail ±3
    return BASE_HEIGHT + baseVal * 10.0f + detailVal * 3.0f;

  case BiomeType::Mountain: {
    // Dramatic: base ±8, plus mountain boost up to +35
    float mountainFactor = std::max(0.0f, mountainVal - 0.2f) / 0.8f;
    mountainFactor *= mountainFactor; // Square for sharper peaks
    return BASE_HEIGHT + baseVal * 8.0f + detailVal * 3.0f +
           mountainFactor * 35.0f;
  }

  default:
    return BASE_HEIGHT + baseVal * 6.0f;
  }
}

// ============================================================================
// Surface Block (biome-specific + mountain stone threshold)
// ============================================================================

BlockType AdvancedTerrainGenerator::GetSurfaceBlock(BiomeType biome, int worldY,
                                                    int terrainHeight) const {
  BiomeInfo info = GetBiomeInfo(biome);

  // Mountains: above a certain height, surface becomes stone
  if (worldY >= info.stoneHeightThreshold) {
    return BlockType::Stone;
  }

  return info.surfaceBlock;
}

// ============================================================================
// Tree Placement (deterministic hash-based)
// ============================================================================

bool AdvancedTerrainGenerator::ShouldPlaceTree(int worldX, int worldZ,
                                               BiomeType biome) const {
  BiomeInfo info = GetBiomeInfo(biome);
  if (info.treeDensity <= 0.0f)
    return false;

  // Best Practice: JITTERED GRID placement (avoids "Madrid grid" pattern)
  // 1. Divide world into cells of CELL_SIZE
  // 2. Each cell gets at most ONE tree
  // 3. Tree position within cell is offset by hash-based jitter
  // 4. We check if THIS world position is the chosen tree spot for its cell

  constexpr int CELL_SIZE = 10; // Cell dimensions

  // Find which cell this position belongs to
  int cellX = (worldX >= 0) ? (worldX / CELL_SIZE)
                            : ((worldX - CELL_SIZE + 1) / CELL_SIZE);
  int cellZ = (worldZ >= 0) ? (worldZ / CELL_SIZE)
                            : ((worldZ - CELL_SIZE + 1) / CELL_SIZE);

  // Hash the cell to get deterministic random values
  uint32_t cellHash = m_Seed * 16777619u;
  cellHash ^= static_cast<uint32_t>(cellX) * 374761393u;
  cellHash ^= static_cast<uint32_t>(cellZ) * 668265263u;
  cellHash = (cellHash ^ (cellHash >> 13)) * 1274126177u;
  cellHash = cellHash ^ (cellHash >> 16);

  // Density check: does this cell even have a tree?
  float chance = static_cast<float>(cellHash & 0xFFFF) / 65536.0f;
  if (chance > info.treeDensity * 15.0f) // Scale density by cell area
    return false;

  // Jittered position within cell: offset ±3 from cell center
  int cellBaseX = cellX * CELL_SIZE;
  int cellBaseZ = cellZ * CELL_SIZE;
  int jitterX = static_cast<int>((cellHash >> 4) & 0x7) - 3; // -3 to +4
  int jitterZ = static_cast<int>((cellHash >> 8) & 0x7) - 3; // -3 to +4
  int treeX = cellBaseX + CELL_SIZE / 2 + jitterX;
  int treeZ = cellBaseZ + CELL_SIZE / 2 + jitterZ;

  // Only the exact jittered position triggers a tree
  return (worldX == treeX && worldZ == treeZ);
}

// ============================================================================
// Cave Generation (placeholder)
// ============================================================================

bool AdvancedTerrainGenerator::HasCaveAt(float x, float y, float z) const {
  return false;
}

// ============================================================================
// Main Generation
// ============================================================================

void AdvancedTerrainGenerator::Generate(Chunk &chunk) const {
  const glm::ivec3 chunkPos = chunk.GetPosition();

  constexpr int DIRT_DEPTH = 4;
  constexpr int BEDROCK_MAX = 5;
  constexpr int MIN_HEIGHT = 51; // BASE(64) - max_amplitude(13)
  constexpr int MAX_HEIGHT =
      107; // BASE(64) + hills(10) + detail(3) + mountain(35)

  int chunkBottomY = chunkPos.y * Chunk::SIZE;
  int chunkTopY = chunkBottomY + Chunk::SIZE - 1;

  // EARLY EXIT: Chunk entirely above max terrain → all air
  if (chunkBottomY > MAX_HEIGHT) {
    return;
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

  // --- Noise Configuration ---

  // Base terrain: broad gentle hills
  FastNoiseLite baseNoise;
  baseNoise.SetSeed(m_Seed);
  baseNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
  baseNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
  baseNoise.SetFractalOctaves(4);
  baseNoise.SetFractalLacunarity(2.0f);
  baseNoise.SetFractalGain(0.5f);
  baseNoise.SetFrequency(0.003f);

  // Detail: fine surface bumps
  FastNoiseLite detailNoise;
  detailNoise.SetSeed(m_Seed + 100);
  detailNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
  detailNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
  detailNoise.SetFractalOctaves(2);
  detailNoise.SetFrequency(0.02f);

  // Mountain: large-scale mountain regions
  FastNoiseLite mountainNoise;
  mountainNoise.SetSeed(m_Seed + 200);
  mountainNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
  mountainNoise.SetFrequency(0.003f);

  // Biome noise: temperature
  FastNoiseLite temperatureNoise;
  temperatureNoise.SetSeed(m_Seed + 300);
  temperatureNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
  temperatureNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
  temperatureNoise.SetFractalOctaves(3);
  temperatureNoise.SetFrequency(0.001f); // Very large-scale biome regions

  // Biome noise: humidity
  FastNoiseLite humidityNoise;
  humidityNoise.SetSeed(m_Seed + 400);
  humidityNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
  humidityNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
  humidityNoise.SetFractalOctaves(3);
  humidityNoise.SetFrequency(0.0015f); // Large-scale

  // --- Pre-compute heightmap and biome map for this chunk ---
  // Store per-column data to allow tree decoration pass after terrain
  struct ColumnData {
    int height;
    BiomeType biome;
  };
  ColumnData columns[Chunk::SIZE][Chunk::SIZE];

  for (int x = 0; x < Chunk::SIZE; x++) {
    for (int z = 0; z < Chunk::SIZE; z++) {
      float worldX = static_cast<float>(chunkPos.x * Chunk::SIZE + x);
      float worldZ = static_cast<float>(chunkPos.z * Chunk::SIZE + z);

      // Sample biome noise
      float temperature = temperatureNoise.GetNoise(worldX, worldZ);
      float humidity = humidityNoise.GetNoise(worldX, worldZ);
      BiomeType biome = GetBiome(temperature, humidity);

      // Sample terrain noise
      float baseVal = baseNoise.GetNoise(worldX, worldZ);
      float detailVal = detailNoise.GetNoise(worldX, worldZ);
      float mountainVal = mountainNoise.GetNoise(worldX, worldZ);

      // Calculate height based on biome
      float height = GetTerrainHeight(baseVal, detailVal, mountainVal, biome);
      int iHeight = static_cast<int>(height);

      columns[x][z] = {iHeight, biome};

      // Fill the column with terrain
      for (int y = 0; y < Chunk::SIZE; y++) {
        int worldY = chunkBottomY + y;

        if (worldY > iHeight) {
          chunk.SetBlock(x, y, z, {BlockType::Air});
        } else {
          if (HasCaveAt(worldX, static_cast<float>(worldY), worldZ)) {
            chunk.SetBlock(x, y, z, {BlockType::Air});
            continue;
          }

          BlockType type;
          if (worldY < BEDROCK_MAX) {
            type = BlockType::Bedrock;
          } else if (worldY == iHeight) {
            type = GetSurfaceBlock(biome, worldY, iHeight);
          } else if (worldY > iHeight - DIRT_DEPTH) {
            type = BlockType::Dirt;
          } else {
            type = BlockType::Stone;
          }

          chunk.SetBlock(x, y, z, {type});
        }
      }
    }
  }

  // --- Tree Decoration Pass (Neighbor-Aware) ---
  // Best Practice: scan BEYOND chunk borders so neighboring trees'
  // canopies extend into this chunk seamlessly. No more grid gaps!
  constexpr int SCAN_MARGIN = 6; // Max canopy radius = 5, + 1 safety
  int chunkWorldX = chunkPos.x * Chunk::SIZE;
  int chunkWorldZ = chunkPos.z * Chunk::SIZE;

  for (int sx = -SCAN_MARGIN; sx < Chunk::SIZE + SCAN_MARGIN; sx++) {
    for (int sz = -SCAN_MARGIN; sz < Chunk::SIZE + SCAN_MARGIN; sz++) {
      int worldX = chunkWorldX + sx;
      int worldZ = chunkWorldZ + sz;

      // Get terrain height at this world position
      // For positions outside chunk, we need to recalculate
      int terrainHeight;
      BiomeType biome;
      if (sx >= 0 && sx < Chunk::SIZE && sz >= 0 && sz < Chunk::SIZE) {
        terrainHeight = columns[sx][sz].height;
        biome = columns[sx][sz].biome;
      } else {
        // Recalculate for out-of-chunk positions
        float fx = static_cast<float>(worldX);
        float fz = static_cast<float>(worldZ);
        float temp = temperatureNoise.GetNoise(fx, fz);
        float hum = humidityNoise.GetNoise(fx, fz);
        biome = GetBiome(temp, hum);
        float bv = baseNoise.GetNoise(fx, fz);
        float dv = detailNoise.GetNoise(fx, fz);
        float mv = mountainNoise.GetNoise(fx, fz);
        terrainHeight = static_cast<int>(GetTerrainHeight(bv, dv, mv, biome));
      }

      if (!ShouldPlaceTree(worldX, worldZ, biome))
        continue;

      // Generate tree blueprint
      auto treeBlocks = OakTreeGenerator::Generate(worldX, worldZ, m_Seed);

      // Place blocks that fall within THIS chunk
      for (const auto &tb : treeBlocks) {
        int bx = sx + tb.dx;
        int by = (terrainHeight + 1 + tb.dy) - chunkBottomY;
        int bz = sz + tb.dz;

        if (Chunk::IsValid(bx, by, bz)) {
          Block existing = chunk.GetBlock(bx, by, bz);
          if (existing.type == BlockType::Air ||
              (tb.type == BlockType::Wood &&
               existing.type == BlockType::Leaves)) {
            chunk.SetBlock(bx, by, bz, {tb.type});
          }
        }
      }
    }
  }
}

// ============================================================================
// Legacy Wave Generator
// ============================================================================

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
