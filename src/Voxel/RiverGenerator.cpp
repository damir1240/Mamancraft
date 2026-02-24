#include "Mamancraft/Voxel/RiverGenerator.hpp"
#include "FastNoiseLite.h"
#include <cmath>

namespace mc {

// ============================================================================
// Implementation detail — two noise layers for organic rivers
// ============================================================================
struct RiverGenerator::Impl {
  FastNoiseLite riverNoise; // Main river path noise
  FastNoiseLite warpNoise;  // Domain warping for more natural curves

  // ── Tuning constants ─────────────────────────────────────────────────────
  // River exists where abs(riverNoise) < RIVER_THRESHOLD
  static constexpr float RIVER_THRESHOLD = 0.045f;

  // Maximum carve depth (at noise==0, center of river)
  static constexpr int MAX_DEPTH = 4;
  // Minimum carve depth (at edge of river)
  static constexpr int MIN_DEPTH = 2;

  // Rivers only form below this elevation (no rivers on mountain peaks)
  static constexpr int MAX_RIVER_ELEVATION = 78;
  // Rivers don't form below sea level
  static constexpr int MIN_RIVER_ELEVATION = 58;

  // Domain warp strength (world units)
  static constexpr float WARP_STRENGTH = 40.0f;

  explicit Impl(uint32_t seed) {
    // River channel noise: low frequency for wide, sweeping curves
    riverNoise.SetSeed(static_cast<int>(seed ^ 0xA1B2C300u));
    riverNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    riverNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    riverNoise.SetFractalOctaves(3);
    riverNoise.SetFrequency(0.003f); // Very low freq = wide spacing

    // Warp noise for organic curves
    warpNoise.SetSeed(static_cast<int>(seed ^ 0xD4E5F600u));
    warpNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    warpNoise.SetFrequency(0.006f);
  }
};

// ============================================================================
RiverGenerator::RiverGenerator(uint32_t seed) : m_Impl(new Impl(seed)) {}

RiverGenerator::~RiverGenerator() { delete m_Impl; }

// ============================================================================
// IsRiverAt — O(1) noise-threshold query
// ============================================================================
//
// How it works:
//   1. Sample warp noise at (worldX, worldZ)
//   2. Apply domain warping to get "warped" coordinates
//   3. Sample river noise at warped coordinates
//   4. If abs(riverNoise) < THRESHOLD and terrain is in valid elevation range:
//      → This column is a river!
//   5. River depth/width scales linearly from edge (abs≈THRESHOLD) to center
//   (abs≈0)
//
// This creates organic, winding river channels that follow noise contour lines.
// The domain warping adds extra irregularity so rivers look natural, not
// sinusoidal.
//
bool RiverGenerator::IsRiverAt(int worldX, int worldZ, int terrainHeight,
                               int &outWaterSurfaceY,
                               int &outRiverDepth) const {
  // Rivers only in moderate elevation range
  if (terrainHeight > Impl::MAX_RIVER_ELEVATION ||
      terrainHeight < Impl::MIN_RIVER_ELEVATION) {
    return false;
  }

  float fx = static_cast<float>(worldX);
  float fz = static_cast<float>(worldZ);

  // Domain warp for organic curves
  float warpX = m_Impl->warpNoise.GetNoise(fx, fz) * Impl::WARP_STRENGTH;
  float warpZ = m_Impl->warpNoise.GetNoise(fx + 5678.0f, fz + 1234.0f) *
                Impl::WARP_STRENGTH;

  // Sample river noise at warped position
  float riverVal = m_Impl->riverNoise.GetNoise(fx + warpX, fz + warpZ);

  float absRiver = std::abs(riverVal);

  if (absRiver >= Impl::RIVER_THRESHOLD) {
    return false; // Not in a river channel
  }

  // ── We're inside a river! ──────────────────────────────────────────────

  // How deep into the channel are we? (0 = edge, 1 = dead center)
  float channelDepth = 1.0f - (absRiver / Impl::RIVER_THRESHOLD);

  // Carve depth: scales from MIN_DEPTH at edge to MAX_DEPTH at center
  int depth =
      Impl::MIN_DEPTH +
      static_cast<int>(channelDepth * (Impl::MAX_DEPTH - Impl::MIN_DEPTH));

  // Water surface sits just below the original terrain surface
  // At the center of the river it's deeper, at edges it's shallower
  outWaterSurfaceY = terrainHeight - 1;
  outRiverDepth = depth;

  return true;
}

} // namespace mc
