#pragma once
#include <cstdint>

// Forward declaration из FastNoiseLite, чтобы не тащить заголовок сюда.
class FastNoiseLite;

namespace mc {

// ============================================================================
// NoiseConfig — иерархическая конфигурация всех шумовых генераторов.
//
// Best Practice: все «магические числа» частот, октав, усилений живут ЗДЕСЬ
// и только ЗДЕСЬ. Меняешь один параметр — видишь весь контекст рядом.
//
// Поля constexpr: значения встраиваются компилятором, нет runtime-оверхeда.
// ============================================================================

namespace noise_cfg {

// --------------------------------------------------------------------------
// Базовый рельеф — широкие холмы и долины
// --------------------------------------------------------------------------
struct Base {
  static constexpr float frequency = 0.003f;
  static constexpr int octaves = 4;
  static constexpr float lacunarity = 2.0f;
  static constexpr float gain = 0.5f;
  static constexpr uint32_t seedOffset = 0u; // m_Seed + seedOffset
};

// --------------------------------------------------------------------------
// Детальный шум — мелкие бугры поверхности
// --------------------------------------------------------------------------
struct Detail {
  static constexpr float frequency = 0.020f;
  static constexpr int octaves = 2;
  static constexpr float lacunarity = 2.0f;
  static constexpr float gain = 0.5f;
  static constexpr uint32_t seedOffset = 100u;
};

// --------------------------------------------------------------------------
// Горный шум — Ridged Multifractal для острых хребтов
// --------------------------------------------------------------------------
struct Mountain {
  static constexpr float frequency = 0.0018f;
  static constexpr int octaves = 5;
  static constexpr float lacunarity = 2.2f;
  static constexpr float gain = 0.5f;
  static constexpr uint32_t seedOffset = 200u;
};

// --------------------------------------------------------------------------
// Domain Warp — органическое искажение координат горного шума
// f_warped(p) = noise(p + c * noise(p)), как описано в отчёте.
// --------------------------------------------------------------------------
struct DomainWarp {
  static constexpr float frequency = 0.0008f;
  static constexpr int octaves = 2;
  static constexpr float lacunarity = 2.0f;
  static constexpr float gain = 0.5f;
  static constexpr float warpStrength = 80.0f; ///< Сила смещения в блоках
  static constexpr uint32_t seedOffset = 500u;
  // Оффсет для второй компоненты warp (Z)
  static constexpr float warpOffsetX = 1234.5f;
  static constexpr float warpOffsetZ = 6789.0f;
};

// --------------------------------------------------------------------------
// Климатические шумы — масштаб биомов (сотни блоков)
// --------------------------------------------------------------------------
struct Temperature {
  static constexpr float frequency = 0.0006f;
  static constexpr int octaves = 3;
  static constexpr float lacunarity = 2.0f;
  static constexpr float gain = 0.5f;
  static constexpr uint32_t seedOffset = 300u;
};

struct Humidity {
  static constexpr float frequency = 0.00065f;
  static constexpr int octaves = 3;
  static constexpr float lacunarity = 2.0f;
  static constexpr float gain = 0.5f;
  static constexpr uint32_t seedOffset = 400u;
};

} // namespace noise_cfg

// ============================================================================
// WorldConstants — глобальные константы мира.
// Изменение одной константы затрагивает все алгоритмы консистентно.
// ============================================================================

namespace world_cfg {

/// Базовая высота поверхности (уровень моря = BASE_HEIGHT)
inline constexpr float BASE_HEIGHT = 64.0f;
/// Ниже этого Y всегда bedrock
inline constexpr int BEDROCK_MAX = 5;
/// Глубина dirt-слоя под grass
inline constexpr int DIRT_DEPTH = 4;
/// Максимальный сдвиг горных хребтов вверх (блоки) — задаёт MAX_HEIGHT
inline constexpr float MOUNTAIN_MAX_AMPLITUDE = 90.0f;
/// Минимально возможная высота terrain
inline constexpr int MIN_HEIGHT = static_cast<int>(BASE_HEIGHT) - 13;
/// Максимально возможная высота terrain
inline constexpr int MAX_HEIGHT = static_cast<int>(BASE_HEIGHT) + 13 + 4 +
                                  static_cast<int>(MOUNTAIN_MAX_AMPLITUDE);
/// Радиус сканирования соседних деревьев через границу чанка
inline constexpr int TREE_SCAN_MARGIN = 6;
/// Размер ячейки решётки для расстановки деревьев (Jittered Grid)
inline constexpr int TREE_CELL_SIZE = 10;

} // namespace world_cfg

} // namespace mc
