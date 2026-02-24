#pragma once
#include "Mamancraft/Voxel/Biome.hpp"
#include <cstdint>

namespace mc {

// ============================================================================
// ColumnSample — результат одного запроса к HeightmapSampler.
//
// Инкапсулирует всё, что нужно знать о вертикальной колонке мира:
// — климат (для биомов и деревьев)
// — высота поверхности (для блочного заполнения)
// — биом (для поверхностных блоков и декораций)
//
// Best Practice: отделяем «что посчитано» от «как посчитано».
// Декораторы (деревья, трава) получают готовый ColumnSample,
// не заботясь о шумах и весах.
// ============================================================================

struct ColumnSample {
  float height; ///< Высота поверхности в блоках (float, округляется при записи)
  ClimateParams climate; ///< Климатические параметры колонки
  BiomeType biome;       ///< Биом (Voronoi nearest-centroid)
};

// ============================================================================
// HeightmapSampler — stateless сэмплер рельефа и климата.
//
// Best Practice (отчёт): отделить «sampling» (кто/где) от «generation» (что
// делать). HeightmapSampler создаётся один раз на сид, затем вызывается для
// любых мировых координат — внутри чанка, снаружи, при сканировании деревьев.
//
// Нет состояния кроме FastNoiseLite объектов → потокобезопасен при чтении.
// Все методы const.
//
// Использование:
//   HeightmapSampler sampler(seed);
//   ColumnSample s = sampler.Sample(worldX, worldZ);
// ============================================================================

class HeightmapSampler {
public:
  explicit HeightmapSampler(uint32_t seed);
  ~HeightmapSampler();

  // Некопируем (содержит FastNoiseLite через pimpl).
  HeightmapSampler(const HeightmapSampler &) = delete;
  HeightmapSampler &operator=(const HeightmapSampler &) = delete;

  // Перемещаем (для хранения в TerrainGenerator).
  HeightmapSampler(HeightmapSampler &&) noexcept;
  HeightmapSampler &operator=(HeightmapSampler &&) noexcept;

  /// @brief Основная точка входа: рассчитать все данные для колонки (x, z).
  /// @param worldX, worldZ  Абсолютные координаты в мире.
  /// @return ColumnSample с высотой, климатом и биомом.
  [[nodiscard]] ColumnSample Sample(float worldX, float worldZ) const;

private:
  // Pimpl — скрываем FastNoiseLite из публичного заголовка.
  // Это позволяет включать HeightmapSampler.hpp без FastNoiseLite.h.
  struct Impl;
  Impl *m_Impl;

  // --- Внутренние хелперы ---

  /// Smoothstep с компиляторными гарантиями inline.
  [[nodiscard]] static float Smoothstep(float edge0, float edge1,
                                        float x) noexcept;

  /// Blended terrain height: взвешенная интерполяция всех биомов по климату.
  [[nodiscard]] float
  ComputeBlendedHeight(float baseVal, float detailVal, float mountainRidgeVal,
                       const ClimateParams &climate) const noexcept;
};

} // namespace mc
