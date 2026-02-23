#include "Mamancraft/Voxel/OakTreeGenerator.hpp"
#include <cmath>

namespace mc {

// ============================================================================
// Hash helper
// ============================================================================

uint32_t OakTreeGenerator::Hash(int x, int z, uint32_t seed) {
  uint32_t h = seed;
  h ^= static_cast<uint32_t>(x) * 374761393u;
  h ^= static_cast<uint32_t>(z) * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}

// ============================================================================
// Place a sphere of leaves centered at (cx, cy, cz) with given radius.
// Best Practice: overlapping sphere clusters = organic canopy.
// ============================================================================

static void PlaceLeafSphere(std::vector<TreeBlock> &blocks, int cx, int cy,
                            int cz, int radius, uint32_t noise) {
  for (int dx = -radius; dx <= radius; dx++) {
    for (int dy = -radius; dy <= radius; dy++) {
      for (int dz = -radius; dz <= radius; dz++) {
        float dist = std::sqrt((float)(dx * dx + dy * dy + dz * dz));
        if (dist > radius)
          continue;

        // Organic edge removal: outer shell is ~20% sparse
        if (dist > radius - 1.0f) {
          uint32_t h = noise;
          h ^= static_cast<uint32_t>(cx + dx + 64) * 5915587277u;
          h ^= static_cast<uint32_t>(cy + dy + 64) * 1500450271u;
          h ^= static_cast<uint32_t>(cz + dz + 64) * 3267000013u;
          h = (h ^ (h >> 16)) * 0x45d9f3b;
          if ((h & 0x3) == 0)
            continue; // 25% removal
        }

        blocks.push_back({cx + dx, cy + dy, cz + dz, BlockType::Leaves});
      }
    }
  }
}

// ============================================================================
// Place a line of Wood blocks from (x0,y0,z0) to (x1,y1,z1) using DDA.
// ============================================================================

static void PlaceBranch(std::vector<TreeBlock> &blocks, int x0, int y0, int z0,
                        int x1, int y1, int z1) {
  int dx = x1 - x0, dy = y1 - y0, dz = z1 - z0;
  int steps = std::max({std::abs(dx), std::abs(dy), std::abs(dz)});
  if (steps == 0)
    return;
  for (int i = 0; i <= steps; i++) {
    float t = (float)i / steps;
    int bx = (int)std::round(x0 + dx * t);
    int by = (int)std::round(y0 + dy * t);
    int bz = (int)std::round(z0 + dz * t);
    blocks.push_back({bx, by, bz, BlockType::Wood});
  }
}

// ============================================================================
// Generate trunk + branches, then place leaf sphere on each branch tip.
//
// Algorithm (research-based):
//   1. Vertical trunk of specified height
//   2. 3-6 branches radiating from upper 40-80% of trunk
//   3. Each branch ends with a leaf sphere cluster
//   4. Additional top cluster over trunk apex
//
// This matches the "Space Colonization" principle: branches grow outward,
// each carries its own leaf cluster → overlapping spheres → organic canopy.
// ============================================================================

std::vector<TreeBlock> OakTreeGenerator::Generate(int worldX, int worldZ,
                                                  uint32_t seed) {
  std::vector<TreeBlock> blocks;
  blocks.reserve(512);

  uint32_t hash = Hash(worldX, worldZ, seed);

  // ── Tree parameters ──────────────────────────────────────────────────────
  // All trees are full-grown oaks (no "small" garbage)
  int trunkHeight = 5 + static_cast<int>((hash >> 2) & 0x3); // 5–8
  int numBranches = 3 + static_cast<int>((hash >> 6) & 0x3); // 3–6
  int leafRadius = 3 + static_cast<int>((hash >> 9) & 0x1);  // 3–4
  int branchLen = 2 + static_cast<int>((hash >> 11) & 0x1);  // 2–3

  // ── Trunk ────────────────────────────────────────────────────────────────
  // Cross-shaped trunk: center + 4 arms for bottom half → natural thick base
  for (int y = 0; y < trunkHeight; y++) {
    blocks.push_back({0, y, 0, BlockType::Wood});
  }
  // Thick base: extra logs for bottom 40%
  int thickTo = (trunkHeight * 4) / 10;
  if (thickTo < 2)
    thickTo = 2;
  for (int y = 0; y < thickTo; y++) {
    blocks.push_back({1, y, 0, BlockType::Wood});
    blocks.push_back({-1, y, 0, BlockType::Wood});
    blocks.push_back({0, y, 1, BlockType::Wood});
    blocks.push_back({0, y, -1, BlockType::Wood});
  }

  // ── Branches + leaf clusters ─────────────────────────────────────────────
  // Branches radiate from upper 40%–90% of trunk at evenly-spaced angles
  // Each branch has a leaf sphere cluster at its tip.

  // Branch origins span from 40% to 90% of trunk height
  float branchStartFrac = 0.40f;
  float branchEndFrac = 0.90f;

  for (int b = 0; b < numBranches; b++) {
    // Height along trunk for this branch (evenly distributed with jitter)
    float frac = branchStartFrac + (branchEndFrac - branchStartFrac) *
                                       ((float)b / (numBranches - 1 + 1));
    int originY = (int)(trunkHeight * frac);

    // Angle around Y axis – evenly spaced + small random jitter
    float baseAngle = (float)b * (6.2831853f / numBranches);
    // Jitter angle using hash bits
    uint32_t bh = hash ^ static_cast<uint32_t>(b) * 2654435761u;
    float jitterAngle = ((float)(bh & 0xFF) / 255.0f - 0.5f) * 0.8f;
    float angle = baseAngle + jitterAngle;

    // Branch tip offset
    int tipX = (int)std::round(std::cos(angle) * branchLen);
    int tipZ = (int)std::round(std::sin(angle) * branchLen);
    // Branches go slightly upward
    int tipY = originY + 1 + static_cast<int>((bh >> 8) & 0x1);

    // Draw branch as line of wood blocks
    PlaceBranch(blocks, 0, originY, 0, tipX, tipY, tipZ);

    // Leaf sphere at branch tip
    PlaceLeafSphere(blocks, tipX, tipY + leafRadius - 1, tipZ, leafRadius,
                    hash ^ static_cast<uint32_t>(b) * 134775813u);
  }

  // ── Top cluster ──────────────────────────────────────────────────────────
  // Crown above trunk apex: slightly larger sphere (main visual mass)
  int topRadius = leafRadius + 1;
  PlaceLeafSphere(blocks, 0, trunkHeight + topRadius - 1, 0, topRadius, hash);

  // ── Deduplicate and prioritize Wood over Leaves ──────────────────────────
  // When a Wood block and Leaf block are at the same position, keep Wood.
  // Simple approach: mark positions, then final-pass filter is done
  // at placement time in TerrainGenerator (wood overwrites leaves).

  return blocks;
}

// ── Unused stubs (interface compliance) ─────────────────────────────────────
void OakTreeGenerator::GenerateTrunk(std::vector<TreeBlock> &, int, int) {}
void OakTreeGenerator::GenerateCanopy(std::vector<TreeBlock> &, int, int,
                                      uint32_t) {}

} // namespace mc
