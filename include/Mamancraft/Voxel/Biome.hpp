#pragma once
#include "Mamancraft/Voxel/Block.hpp"
#include <array>
#include <cstdint>
#include <string_view>

namespace mc {

// ============================================================================
// BiomeType — перечисление биомов.
// Намеренно НЕ содержит "River" — реки больше не биом, а отдельный
// топологический слой (согласно отчёту Best Practice 2026).
// ============================================================================

enum class BiomeType : uint8_t {
  Plains = 0,
  OakForest,
  Mountain,
  Desert, // Зарезервировано
  Tundra, // Зарезервировано
  Count
};

// ============================================================================
// ClimateParams — климатические параметры колонки в пространстве Whittaker.
// Все поля в диапазоне [-1, +1] (нормированный выход шума OpenSimplex2).
//
// Best Practice: хранить климат как отдельную структуру данных,
// не смешивая с геометрией. Это позволяет переиспользовать климат
// для генерации биомов, деревьев, осадков, снега и т.д.
// ============================================================================

struct ClimateParams {
  float temperature; ///< [-1, +1]: -1=арктика, +1=тропики
  float humidity;    ///< [-1, +1]: -1=пустыня, +1=тропический лес
};

// ============================================================================
// BiomeDefinition — полное описание биома.
//
// Best Practice: данные биома отделены от логики генерации (Data-Oriented).
// Добавление нового биома = добавление одной записи в таблицу,
// без касания алгоритмов.
// ============================================================================

struct BiomeDefinition {
  BiomeType type;
  std::string_view name;

  // --- Климатические центроиды (Voronoi, Whittaker-диаграмма) ---
  float centerTemperature; ///< Идеальная температура биома
  float centerHumidity;    ///< Идеальная влажность биома

  // --- Параметры рельефа ---
  float baseAmplitude;     ///< Амплитуда базового шума (холмы)
  float detailAmplitude;   ///< Амплитуда детального шума (бугры)
  float mountainAmplitude; ///< Амплитуда горного шума (хребты)
  float baseHeight;        ///< Базовая высота поверхности (блоки)

  // --- Параметры поверхности ---
  BlockType surfaceBlock;    ///< Верхний блок (трава, песок и т.д.)
  BlockType subSurfaceBlock; ///< Подповерхностный блок (земля, камень)
  int subSurfaceDepth;       ///< Глубина подповерхностного слоя (блоки)

  // --- Декорации ---
  float treeDensity;    ///< Вероятность дерева [0..1]
  int stoneStartFactor; ///< Процент высоты над BASE_HEIGHT, с которого идёт
                        ///< камень (999 = нет камня на поверхности)
};

// ============================================================================
// kBiomeTable — compile-time таблица всех биомов.
// ============================================================================

inline constexpr uint8_t kBiomeCount = static_cast<uint8_t>(BiomeType::Count);

inline constexpr std::array<BiomeDefinition, kBiomeCount> kBiomeTable = {{
    {BiomeType::Plains, "Plains", -0.30f, -0.20f, 5.0f, 1.5f, 0.0f, 64.0f,
     BlockType::Grass, BlockType::Dirt, 4, 0.00f, 999},
    {BiomeType::OakForest, "OakForest", 0.15f, 0.20f, 10.0f, 3.0f, 0.0f, 64.0f,
     BlockType::Grass, BlockType::Dirt, 4, 0.08f, 999},
    {BiomeType::Mountain, "Mountain", 0.60f, 0.60f, 12.0f, 4.0f, 90.0f, 64.0f,
     BlockType::Grass, BlockType::Dirt, 4, 0.00f, 65},
    {BiomeType::Desert, "Desert", 0.70f, -0.70f, 4.0f, 1.0f, 0.0f, 63.0f,
     BlockType::Sand, BlockType::Sand, 6, 0.00f, 999},
    {BiomeType::Tundra, "Tundra", -0.70f, 0.40f, 3.0f, 0.5f, 0.0f, 64.0f,
     BlockType::Grass, BlockType::Dirt, 4, 0.00f, 999},
}};

static_assert(kBiomeTable.size() == kBiomeCount,
              "kBiomeTable must have an entry for every BiomeType!");

[[nodiscard]] inline constexpr const BiomeDefinition &
GetBiomeDef(BiomeType type) noexcept {
  return kBiomeTable[static_cast<uint8_t>(type)];
}

[[nodiscard]] inline BiomeType ClassifyBiome(const ClimateParams &c) noexcept {
  float bestDist = 1e9f;
  BiomeType bestBiome = BiomeType::Plains;

  for (uint8_t i = 0; i < kBiomeCount; ++i) {
    const auto &def = kBiomeTable[i];
    float dt = c.temperature - def.centerTemperature;
    float dh = c.humidity - def.centerHumidity;
    float dist = dt * dt + dh * dh;
    if (dist < bestDist) {
      bestDist = dist;
      bestBiome = def.type;
    }
  }
  return bestBiome;
}

struct BiomeInfo {
  BiomeType type;
  BlockType surfaceBlock;
  BlockType subSurfaceBlock;
  float treeDensity;
  int stoneHeightThreshold;
};

[[nodiscard]] inline BiomeInfo GetBiomeInfo(BiomeType biome) noexcept {
  const auto &def = GetBiomeDef(biome);
  return BiomeInfo{
      .type = def.type,
      .surfaceBlock = def.surfaceBlock,
      .subSurfaceBlock = def.subSurfaceBlock,
      .treeDensity = def.treeDensity,
      .stoneHeightThreshold = def.stoneStartFactor,
  };
}

} // namespace mc
