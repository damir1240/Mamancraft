#include "Mamancraft/Voxel/HeightmapSampler.hpp"
#include "FastNoiseLite.h"
#include "Mamancraft/Voxel/NoiseConfig.hpp"
#include <algorithm>
#include <cmath>

namespace mc {

// ============================================================================
// Impl — хранит все FastNoiseLite объекты (скрыты через pimpl).
// Инициализируется один раз в конструкторе HeightmapSampler.
// ============================================================================

struct HeightmapSampler::Impl {
  // Каждый генератор шума инициализируется через makeFNL() ниже.
  FastNoiseLite base;        ///< Широкие холмы и долины
  FastNoiseLite detail;      ///< Мелкие бугры поверхности
  FastNoiseLite mountain;    ///< Ridged Multifractal — острые хребты
  FastNoiseLite warp;        ///< Domain Warp для координат mountain
  FastNoiseLite temperature; ///< Климатическая ось температуры
  FastNoiseLite humidity;    ///< Климатическая ось влажности

  explicit Impl(uint32_t seed) {
    using namespace noise_cfg;

    // ── Базовый рельеф ────────────────────────────────────────────────────
    base.SetSeed(static_cast<int>(seed + Base::seedOffset));
    base.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    base.SetFractalType(FastNoiseLite::FractalType_FBm);
    base.SetFractalOctaves(Base::octaves);
    base.SetFractalLacunarity(Base::lacunarity);
    base.SetFractalGain(Base::gain);
    base.SetFrequency(Base::frequency);

    // ── Детальный шум ─────────────────────────────────────────────────────
    detail.SetSeed(static_cast<int>(seed + Detail::seedOffset));
    detail.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    detail.SetFractalType(FastNoiseLite::FractalType_FBm);
    detail.SetFractalOctaves(Detail::octaves);
    detail.SetFractalLacunarity(Detail::lacunarity);
    detail.SetFractalGain(Detail::gain);
    detail.SetFrequency(Detail::frequency);

    // ── Горный (Ridged Multifractal) ──────────────────────────────────────
    // Best Practice: FractalType_Ridged инвертирует |abs(noise)|,
    // создавая острые пики вместо округлых холмов.
    mountain.SetSeed(static_cast<int>(seed + Mountain::seedOffset));
    mountain.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    mountain.SetFractalType(FastNoiseLite::FractalType_Ridged);
    mountain.SetFractalOctaves(Mountain::octaves);
    mountain.SetFractalLacunarity(Mountain::lacunarity);
    mountain.SetFractalGain(Mountain::gain);
    mountain.SetFrequency(Mountain::frequency);

    // ── Domain Warp ───────────────────────────────────────────────────────
    // f_warped(p) = mountain(p + warpStrength * warp(p))
    // Разрывает регулярный паттерн шума → органические горные цепи.
    warp.SetSeed(static_cast<int>(seed + DomainWarp::seedOffset));
    warp.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    warp.SetFractalType(FastNoiseLite::FractalType_FBm);
    warp.SetFractalOctaves(DomainWarp::octaves);
    warp.SetFractalLacunarity(DomainWarp::lacunarity);
    warp.SetFractalGain(DomainWarp::gain);
    warp.SetFrequency(DomainWarp::frequency);

    // ── Климатические шумы (очень низкая частота — масштаб биомов) ───────
    temperature.SetSeed(static_cast<int>(seed + Temperature::seedOffset));
    temperature.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    temperature.SetFractalType(FastNoiseLite::FractalType_FBm);
    temperature.SetFractalOctaves(Temperature::octaves);
    temperature.SetFractalLacunarity(Temperature::lacunarity);
    temperature.SetFractalGain(Temperature::gain);
    temperature.SetFrequency(Temperature::frequency);

    humidity.SetSeed(static_cast<int>(seed + Humidity::seedOffset));
    humidity.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    humidity.SetFractalType(FastNoiseLite::FractalType_FBm);
    humidity.SetFractalOctaves(Humidity::octaves);
    humidity.SetFractalLacunarity(Humidity::lacunarity);
    humidity.SetFractalGain(Humidity::gain);
    humidity.SetFrequency(Humidity::frequency);
  }
};

// ============================================================================
// HeightmapSampler — implementation
// ============================================================================

HeightmapSampler::HeightmapSampler(uint32_t seed) : m_Impl(new Impl(seed)) {}

HeightmapSampler::~HeightmapSampler() { delete m_Impl; }

HeightmapSampler::HeightmapSampler(HeightmapSampler &&other) noexcept
    : m_Impl(other.m_Impl) {
  other.m_Impl = nullptr;
}

HeightmapSampler &
HeightmapSampler::operator=(HeightmapSampler &&other) noexcept {
  if (this != &other) {
    delete m_Impl;
    m_Impl = other.m_Impl;
    other.m_Impl = nullptr;
  }
  return *this;
}

// ============================================================================
// Sample — основная точка входа.
// Порядок вычислений намеренно инкапсулирован здесь; вызывающий код
// получает только готовый ColumnSample.
// ============================================================================

ColumnSample HeightmapSampler::Sample(float worldX, float worldZ) const {
  using namespace noise_cfg;

  // 1. Климатические оси
  ClimateParams climate{
      .temperature = m_Impl->temperature.GetNoise(worldX, worldZ),
      .humidity = m_Impl->humidity.GetNoise(worldX, worldZ),
  };

  // 2. Biome Voronoi (nearest centroid)
  BiomeType biome = ClassifyBiome(climate);

  // 3. Базовый и детальный шум рельефа
  float baseVal = m_Impl->base.GetNoise(worldX, worldZ);
  float detailVal = m_Impl->detail.GetNoise(worldX, worldZ);

  // 4. Domain Warp координат для горного шума
  //    f_warped(p) = noise(p + strength * noise(p + offset))
  //    Два отдельных вызова с разными оффсетами дают независимые X и Z
  //    компоненты вектора смещения (идиома из отчёта).
  float warpX =
      m_Impl->warp.GetNoise(worldX, worldZ) * DomainWarp::warpStrength;
  float warpZ = m_Impl->warp.GetNoise(worldX + DomainWarp::warpOffsetX,
                                      worldZ + DomainWarp::warpOffsetZ) *
                DomainWarp::warpStrength;

  // Нормализуем ridged noise в [0, 1]: FastNoiseLite Ridged → [-1, 1]
  float mountainRaw =
      (m_Impl->mountain.GetNoise(worldX + warpX, worldZ + warpZ) + 1.0f) * 0.5f;

  // 5. Blended height
  float h = ComputeBlendedHeight(baseVal, detailVal, mountainRaw, climate);

  return ColumnSample{.height = h, .climate = climate, .biome = biome};
}

// ============================================================================
// ComputeBlendedHeight — взвешенная интерполяция высоты биомов.
//
// Best Practice (отчёт §«Fast Biome Blending Without Squareness»):
//   — Каждый биом вносит свой вклад в высоту через плавный вес [0..1].
//   — Веса определяются через smoothstep по климатическим осям.
//   — Финальная высота = weighted average всех биомов.
//   — Первые производные поверхности непрерывны → нет видимых швов.
//
// Data-driven вариант: читаем параметры амплитуды из kBiomeTable,
// не хардкодим их здесь. Добавление биома = строка в таблице.
// ============================================================================

float HeightmapSampler::ComputeBlendedHeight(
    float baseVal, float detailVal, float mountainRidgeVal,
    const ClimateParams &c) const noexcept {

  // ── Веса биомов через smoothstep ─────────────────────────────────────────
  float wMountain = Smoothstep(0.45f, 0.75f, c.temperature);

  float wPlainsTemp = Smoothstep(-0.15f, -0.50f, c.temperature);
  float wPlainsHum = Smoothstep(-0.20f, -0.60f, c.humidity);
  float wPlains = std::max(wPlainsTemp, wPlainsHum);
  wPlains = std::max(0.0f, wPlains - wMountain);

  float wForest = std::max(0.0f, 1.0f - wMountain - wPlains);

  float totalWeight = wMountain + wPlains + wForest;
  if (totalWeight < 1e-6f) {
    // Fallback: равнина
    const auto &def = GetBiomeDef(BiomeType::Plains);
    return def.baseHeight + baseVal * def.baseAmplitude +
           detailVal * def.detailAmplitude;
  }

  // ── Высота каждого биома из kBiomeTable ───────────────────────────────────
  auto heightForBiome = [&](BiomeType bt) -> float {
    const auto &def = GetBiomeDef(bt);
    float h = def.baseHeight + baseVal * def.baseAmplitude +
              detailVal * def.detailAmplitude;
    if (def.mountainAmplitude > 0.0f) {
      // Для горных биомов: кривая третьей степени заостряет пики
      float ridge = mountainRidgeVal * mountainRidgeVal * mountainRidgeVal;
      h += ridge * def.mountainAmplitude;
    }
    return h;
  };

  float hPlains = heightForBiome(BiomeType::Plains);
  float hForest = heightForBiome(BiomeType::OakForest);
  float hMountain = heightForBiome(BiomeType::Mountain);

  return (wPlains * hPlains + wForest * hForest + wMountain * hMountain) /
         totalWeight;
}

// ============================================================================
// Smoothstep — inline helper, идентичен GLSL smoothstep().
// ============================================================================

float HeightmapSampler::Smoothstep(float edge0, float edge1, float x) noexcept {
  float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

} // namespace mc
