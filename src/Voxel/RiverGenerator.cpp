#include "Mamancraft/Voxel/RiverGenerator.hpp"
#include "FastNoiseLite.h"
#include <cmath>

namespace mc {

// ============================================================================
// Implementation — multi-layer noise for realistic segmented rivers
// ============================================================================
struct RiverGenerator::Impl {
  FastNoiseLite riverNoise; // Primary river path (winding bands)
  FastNoiseLite warpNoise;  // Domain warping for organic curves
  FastNoiseLite maskNoise; // Segments: breaks infinite bands into finite rivers
  FastNoiseLite widthNoise; // Per-river width variation (tributaries vs main)
  FastNoiseLite tributaryNoise; // Secondary smaller rivers (tributary layer)
  FastNoiseLite tribWarpNoise;  // Domain warp for tributaries

  // ── Main river tuning ────────────────────────────────────────────────────
  static constexpr float RIVER_THRESHOLD =
      0.035f; // Where abs(noise) < this = river
  static constexpr float WARP_STRENGTH = 50.0f; // Domain warp magnitude

  // ── Segment mask ─────────────────────────────────────────────────────────
  // maskNoise > MASK_CUTOFF → river is blocked (creates gap = river "ends")
  // This breaks infinite noise bands into finite river segments
  static constexpr float MASK_CUTOFF = 0.08f;

  // ── Tributary tuning ─────────────────────────────────────────────────────
  static constexpr float TRIB_THRESHOLD = 0.020f; // Narrower than main rivers
  static constexpr float TRIB_WARP_STRENGTH = 30.0f;

  // ── Depth / elevation ────────────────────────────────────────────────────
  static constexpr int MAX_DEPTH = 4;
  static constexpr int MIN_DEPTH = 2;
  static constexpr int MAX_RIVER_ELEVATION = 80;
  static constexpr int MIN_RIVER_ELEVATION = 56;

  explicit Impl(uint32_t seed) {
    // ── Main river noise ───────────────────────────────────────────────────
    // Low frequency → wide sweeping curves, long rivers
    riverNoise.SetSeed(static_cast<int>(seed ^ 0xA1B2C3D4u));
    riverNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    riverNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    riverNoise.SetFractalOctaves(2);
    riverNoise.SetFrequency(0.0015f); // Very low → rivers far apart

    // ── Warp noise (adds meanders to the main river) ───────────────────────
    warpNoise.SetSeed(static_cast<int>(seed ^ 0xD4E5F678u));
    warpNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    warpNoise.SetFrequency(0.004f);

    // ── Segment mask: breaks bands into finite rivers ─────────────────────
    // Very low frequency: each "alive" region = one river segment (~200-400
    // blocks long)
    maskNoise.SetSeed(static_cast<int>(seed ^ 0x12345678u));
    maskNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    maskNoise.SetFrequency(0.0008f); // Extremely low → large segments

    // ── Width variation: some sections wider (main) some narrow (near source)
    // ──
    widthNoise.SetSeed(static_cast<int>(seed ^ 0x87654321u));
    widthNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    widthNoise.SetFrequency(0.005f);

    // ── Tributary layer: secondary rivers, higher frequency ────────────────
    tributaryNoise.SetSeed(static_cast<int>(seed ^ 0xFEDCBA98u));
    tributaryNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    tributaryNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    tributaryNoise.SetFractalOctaves(2);
    tributaryNoise.SetFrequency(0.004f); // Higher freq → more frequent, shorter

    tribWarpNoise.SetSeed(static_cast<int>(seed ^ 0x11223344u));
    tribWarpNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    tribWarpNoise.SetFrequency(0.008f);
  }
};

// ============================================================================
RiverGenerator::RiverGenerator(uint32_t seed) : m_Impl(new Impl(seed)) {}

RiverGenerator::~RiverGenerator() { delete m_Impl; }

// ============================================================================
// IsRiverAt — O(1) per column, dual-layer (main rivers + tributaries)
// ============================================================================
bool RiverGenerator::IsRiverAt(int worldX, int worldZ, int terrainHeight,
                               int &outWaterSurfaceY,
                               int &outRiverDepth) const {
  // Elevation check: no rivers on peaks or below sea level
  if (terrainHeight > Impl::MAX_RIVER_ELEVATION ||
      terrainHeight < Impl::MIN_RIVER_ELEVATION) {
    return false;
  }

  float fx = static_cast<float>(worldX);
  float fz = static_cast<float>(worldZ);

  // ── Layer 1: Main rivers ─────────────────────────────────────────────────
  {
    // Domain warp
    float warpX = m_Impl->warpNoise.GetNoise(fx, fz) * Impl::WARP_STRENGTH;
    float warpZ = m_Impl->warpNoise.GetNoise(fx + 5678.0f, fz + 1234.0f) *
                  Impl::WARP_STRENGTH;

    float riverVal = m_Impl->riverNoise.GetNoise(fx + warpX, fz + warpZ);
    float absRiver = std::abs(riverVal);

    // Segment mask: is this part of the noise band "alive" or "cut"?
    float maskVal = m_Impl->maskNoise.GetNoise(fx, fz);

    // Width variation: slightly widen/narrow the threshold
    float widthMod = m_Impl->widthNoise.GetNoise(fx, fz);
    float localThreshold = Impl::RIVER_THRESHOLD * (1.0f + widthMod * 0.3f);

    // River exists where: abs(river) < threshold AND mask permits it
    if (absRiver < localThreshold && maskVal < Impl::MASK_CUTOFF) {
      float channelDepth = 1.0f - (absRiver / localThreshold);
      int depth =
          Impl::MIN_DEPTH +
          static_cast<int>(channelDepth * (Impl::MAX_DEPTH - Impl::MIN_DEPTH));

      outWaterSurfaceY = terrainHeight - 1;
      outRiverDepth = depth;
      return true;
    }
  }

  // ── Layer 2: Tributaries (smaller, more frequent) ────────────────────────
  {
    float twX =
        m_Impl->tribWarpNoise.GetNoise(fx, fz) * Impl::TRIB_WARP_STRENGTH;
    float twZ = m_Impl->tribWarpNoise.GetNoise(fx + 3333.0f, fz + 7777.0f) *
                Impl::TRIB_WARP_STRENGTH;

    float tribVal = m_Impl->tributaryNoise.GetNoise(fx + twX, fz + twZ);
    float absTrib = std::abs(tribVal);

    // Tributaries also use the mask, but inverted partially — they only show
    // NEAR main rivers. We use the mask to suppress tributaries in areas
    // where there is no main river nearby.
    float maskVal = m_Impl->maskNoise.GetNoise(fx, fz);

    // Tributaries appear when mask is moderate (near river edges)
    // and abs is below narrow threshold
    if (absTrib < Impl::TRIB_THRESHOLD && maskVal < Impl::MASK_CUTOFF + 0.15f) {
      float channelDepth = 1.0f - (absTrib / Impl::TRIB_THRESHOLD);
      int depth = Impl::MIN_DEPTH +
                  static_cast<int>(channelDepth * 1.0f); // max 3 blocks deep

      outWaterSurfaceY = terrainHeight - 1;
      outRiverDepth = depth;
      return true;
    }
  }

  return false;
}

} // namespace mc
