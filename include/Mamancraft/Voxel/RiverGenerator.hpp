#pragma once
#include <cstdint>

namespace mc {

/**
 * RiverGenerator — Noise-threshold river system (O(1) per column).
 *
 * Best practice approach used in Minecraft-style games:
 *   1. A dedicated low-frequency "river noise" generates a continuous field.
 *   2. Where abs(riverNoise) < THRESHOLD, there is a river channel.
 *   3. The closer to 0 the noise value, the deeper/wider the channel.
 *   4. Rivers naturally form winding bands through the terrain.
 *   5. River depth is carved relative to terrain height at that column.
 *
 * This is fully deterministic, context-free (no path tracing), and
 * requires only ONE noise sample per column — zero performance impact.
 */
class RiverGenerator {
public:
  explicit RiverGenerator(uint32_t seed);
  ~RiverGenerator();

  /**
   * Check if (worldX, worldZ) is inside a river channel.
   *
   * @param worldX, worldZ  World-space column coordinates
   * @param terrainHeight   Terrain surface Y at this column
   * @param outWaterSurfaceY  [out] Y level of water surface
   * @param outRiverDepth     [out] how many blocks deep the river is carved
   * @return true if this column is part of a river
   */
  bool IsRiverAt(int worldX, int worldZ, int terrainHeight,
                 int &outWaterSurfaceY, int &outRiverDepth) const;

private:
  struct Impl;
  Impl *m_Impl;
};

} // namespace mc
