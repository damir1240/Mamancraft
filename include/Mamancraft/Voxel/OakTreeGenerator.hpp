#pragma once
#include "Mamancraft/Voxel/Block.hpp"
#include <cstdint>
#include <vector>

namespace mc {

// Single block placement relative to tree base (y=0 = surface block)
struct TreeBlock {
  int dx, dy, dz;
  BlockType type;
};

/**
 * @brief Generates procedural oak trees using branch + leaf-sphere clusters.
 *
 * Algorithm (based on Space Colonization research):
 *   - Thick trunk (cross-shaped base)
 *   - 3-6 branches radiating outward at different heights and angles
 *   - Each branch tip gets a sphere of leaves
 *   - Top cluster above trunk apex (main canopy mass)
 *   - Overlapping spheres = organic, non-uniform canopy
 *
 * All trees are deterministic based on worldX, worldZ, seed.
 * Trees vary in trunk height (5-8), number of branches (3-6),
 * leaf sphere radius (3-4), and branch angles.
 */
class OakTreeGenerator {
public:
  static std::vector<TreeBlock> Generate(int worldX, int worldZ, uint32_t seed);
  static uint32_t Hash(int x, int z, uint32_t seed);

private:
  // Legacy stubs (unused, kept for API compat)
  static void GenerateTrunk(std::vector<TreeBlock> &blocks, int height,
                            int variant);
  static void GenerateCanopy(std::vector<TreeBlock> &blocks, int trunkHeight,
                             int canopyRadius, uint32_t hash);
};

} // namespace mc
