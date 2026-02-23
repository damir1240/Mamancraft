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
  // Used for tree placement and surface block logic only.
  // Terrain HEIGHT uses smooth blending (see GetTerrainHeight below).
  if (temperature > 0.35f)
    return BiomeType::Mountain;
  if (temperature < -0.2f || humidity < -0.3f)
    return BiomeType::Plains;
  return BiomeType::OakForest;
}

// ============================================================================
// Terrain Height — SMOOTH BIOME BLENDING
// ============================================================================
//
// Best Practice: instead of switching height formulas at a hard boundary,
// we give each biome a continuous influence weight derived from noise values,
// then lerp between all biome heights proportionally.
//
// Weight functions use smoothstep so influence ramps up/down gradually.
// Result: mountain peaks naturally taper into forest hills, which gently
// flatten into plains — no visible seam anywhere.
// ============================================================================

static float smoothstep(float edge0, float edge1, float x) {
  float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

float AdvancedTerrainGenerator::GetTerrainHeight(float baseVal, float detailVal,
                                                 float mountainVal,
                                                 BiomeType /*unused*/) const {
  // Legacy stub — real work done by GetTerrainHeightBlended.
  constexpr float BASE_HEIGHT = 64.0f;
  return BASE_HEIGHT + baseVal * 8.0f + detailVal * 3.0f;
}

float AdvancedTerrainGenerator::GetTerrainHeightBlended(float baseVal,
                                                        float detailVal,
                                                        float mountainRidgeVal,
                                                        float temperature,
                                                        float humidity) const {

  constexpr float BASE_HEIGHT = 64.0f;

  // ── Per-biome height contributions ──────────────────────────────────────
  float heightPlains = BASE_HEIGHT + baseVal * 5.0f + detailVal * 1.5f;
  float heightForest = BASE_HEIGHT + baseVal * 10.0f + detailVal * 3.0f;

  // Mountain: Ridged Multifractal noise produces sharp ridge lines.
  // mountainRidgeVal is in [0,1] (already abs-inverted by FractalType_Ridged):
  //   values close to 1.0 = ridge peaks, values near 0 = valleys
  //
  // We apply a power curve to sharpen peaks further, then scale to height.
  float ridgePeak = mountainRidgeVal;                  // [0, 1] ridge value
  ridgePeak = ridgePeak * ridgePeak * ridgePeak;       // sharpen: power 3
  float heightMountain = BASE_HEIGHT + baseVal * 12.0f // broad base shape
                         + detailVal * 4.0f            // surface detail
                         + ridgePeak * 90.0f;          // dramatic ridge peaks

  // ── Biome weights ───────────────────────────────────────────────────────
  float wMountain = smoothstep(0.20f, 0.50f, temperature);
  float wPlainsTemp = smoothstep(-0.10f, -0.35f, temperature);
  float wPlainsHum = smoothstep(-0.15f, -0.40f, humidity);
  float wPlains = std::max(wPlainsTemp, wPlainsHum);
  wPlains = std::max(0.0f, wPlains - wMountain);
  float wForest = std::max(0.0f, 1.0f - wMountain - wPlains);

  float totalWeight = wMountain + wPlains + wForest;
  if (totalWeight < 1e-6f)
    return heightForest;

  return (wMountain * heightMountain + wPlains * heightPlains +
          wForest * heightForest) /
         totalWeight;
}

// ============================================================================
// Surface Block (biome-specific + mountain stone threshold)
// ============================================================================

BlockType AdvancedTerrainGenerator::GetSurfaceBlock(BiomeType biome, int worldY,
                                                    int terrainHeight) const {
  BiomeInfo info = GetBiomeInfo(biome);

  // Mountains: stone appears on the UPPER part of mountain terrain.
  // Instead of a fixed absolute Y, use relative fraction of terrain height.
  // This ensures stone caps appear only near peaks, not at all altitude.
  if (biome == BiomeType::Mountain) {
    constexpr float BASE_HEIGHT = 64.0f;
    // Stone starts at ~65% above base (so foothills stay grassy)
    float stoneStart = BASE_HEIGHT + (terrainHeight - BASE_HEIGHT) * 0.65f;
    // Hard minimum: never stone below Y=85
    stoneStart = std::max(stoneStart, 85.0f);
    if (worldY >= static_cast<int>(stoneStart)) {
      return BlockType::Stone;
    }
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

  // Mountain: Ridged Multifractal — creates sharp peaks and ridge lines
  // Best Practice: FractalType_Ridged inverts the absolute value, giving
  // spike-like peaks instead of smooth bumps. Combined with domain warping,
  // this produces natural mountain chains with varied peak heights.
  FastNoiseLite mountainNoise;
  mountainNoise.SetSeed(m_Seed + 200);
  mountainNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
  mountainNoise.SetFractalType(
      FastNoiseLite::FractalType_Ridged); // KEY: ridges!
  mountainNoise.SetFractalOctaves(5); // More octaves = more detail on slopes
  mountainNoise.SetFractalLacunarity(2.2f);
  mountainNoise.SetFractalGain(0.5f);
  mountainNoise.SetFrequency(0.0018f); // Mid-scale: mountain ranges

  // Domain Warp noise: displaces mountain sampling coordinates organically.
  // This breaks up the regular noise pattern into flowing ridges and chains.
  FastNoiseLite warpNoise;
  warpNoise.SetSeed(m_Seed + 500);
  warpNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
  warpNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
  warpNoise.SetFractalOctaves(2);
  warpNoise.SetFrequency(0.0008f); // Warp at large scale = mountain chains

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

      // Sample base terrain noise
      float baseVal = baseNoise.GetNoise(worldX, worldZ);
      float detailVal = detailNoise.GetNoise(worldX, worldZ);

      // Sample mountain noise with DOMAIN WARPING:
      // warp the x,z coords by a second noise function before sampling
      // mountain. This creates natural mountain chains and irregular ridge
      // formations instead of uniform blobs.
      constexpr float WARP_STRENGTH =
          80.0f; // How far (in world units) to shift
      float warpX = warpNoise.GetNoise(worldX, worldZ) * WARP_STRENGTH;
      float warpZ = warpNoise.GetNoise(worldX + 1234.5f, worldZ + 6789.0f) *
                    WARP_STRENGTH;
      float mountainVal =
          (mountainNoise.GetNoise(worldX + warpX, worldZ + warpZ) + 1.0f) *
          0.5f;
      // mountainVal is now in [0, 1]: 1.0 = sharp ridge peak, 0.0 = valley

      // Calculate height with SMOOTH BIOME BLENDING
      // This is the key fix: instead of picking one biome height formula,
      // we blend all three biomes smoothly based on noise weights.
      float height = GetTerrainHeightBlended(baseVal, detailVal, mountainVal,
                                             temperature, humidity);
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
        float wxv = warpNoise.GetNoise(fx, fz) * 80.0f;
        float wzv = warpNoise.GetNoise(fx + 1234.5f, fz + 6789.0f) * 80.0f;
        float mv = (mountainNoise.GetNoise(fx + wxv, fz + wzv) + 1.0f) * 0.5f;
        terrainHeight =
            static_cast<int>(GetTerrainHeightBlended(bv, dv, mv, temp, hum));
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
