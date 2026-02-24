#pragma once
#include <array>
#include <cstdint>

namespace mc {

/// @brief Тип блока. uint8_t — влезает в Layer Map без оверхeда.
enum class BlockType : uint8_t {
  Air = 0,
  Dirt,
  Grass,
  Stone,
  Wood,
  Leaves,
  Bedrock,
  Water,
  Sand,   // Зарезервировано: береговая зона, дельты рек
  Gravel, // Зарезервировано: русло рек, дно водоёмов
  Count
};

// ============================================================================
// Физические свойства блока (Best Practice: данные отделены от типа)
// Используются слоями симуляции (эрозия, оползни).
// Хранятся в compile-time таблице → нет vtable, нет динамической
// диспетчеризации.
// ============================================================================

/// @brief Физические свойства одного типа блока.
struct BlockProperties {
  float density;      ///< г/см³ (условно). Влияет на массу слоя.
  float hardness;     ///< Устойчивость к эрозии [0..1]: 0=рыхлый, 1=монолит.
  float talusAngle;   ///< Угол естественного откоса, градусы. Для термической
                      ///< эрозии.
  float dissolveRate; ///< Скорость растворения водой [0..1]. Для гидравлической
                      ///< эрозии.
  bool isLiquid;      ///< Является ли блок жидкостью.
  bool isOpaque;      ///< Непрозрачность (для мешинга).
};

/// @brief Compile-time таблица свойств всех блоков.
///        Индексируется через static_cast<uint8_t>(BlockType).
constexpr BlockProperties kBlockProperties[] = {
    //           density  hardness  talus  dissolve  isLiquid  isOpaque
    /* Air     */ {0.0f, 0.0f, 0.0f, 0.0f, false, false},
    /* Dirt    */ {1.5f, 0.35f, 45.0f, 0.25f, false, true},
    /* Grass   */ {1.3f, 0.30f, 42.0f, 0.20f, false, true},
    /* Stone   */ {2.7f, 0.90f, 85.0f, 0.02f, false, true},
    /* Wood    */ {0.6f, 0.70f, 80.0f, 0.05f, false, true},
    /* Leaves  */ {0.1f, 0.05f, 30.0f, 0.30f, false, false},
    /* Bedrock */ {3.0f, 1.00f, 90.0f, 0.00f, false, true},
    /* Water   */ {1.0f, 0.00f, 0.0f, 0.00f, true, false},
    /* Sand    */ {1.6f, 0.10f, 34.0f, 0.40f, false, true},
    /* Gravel  */ {1.8f, 0.20f, 39.0f, 0.15f, false, true},
};
static_assert(std::size(kBlockProperties) ==
                  static_cast<uint8_t>(BlockType::Count),
              "kBlockProperties must have an entry for every BlockType!");

/// @brief Быстрый доступ к свойствам блока по типу. Inline → zero-cost.
[[nodiscard]] inline constexpr const BlockProperties &
GetBlockProperties(BlockType type) noexcept {
  return kBlockProperties[static_cast<uint8_t>(type)];
}

// ============================================================================
// Block — минимальная структура воксельного элемента.
// Намеренно POD: хранится в плотных массивах чанков.
// ============================================================================

struct Block {
  BlockType type = BlockType::Air;

  [[nodiscard]] constexpr bool IsOpaque() const noexcept {
    return GetBlockProperties(type).isOpaque;
  }

  [[nodiscard]] constexpr bool IsSolid() const noexcept {
    return type != BlockType::Air && !GetBlockProperties(type).isLiquid;
  }

  [[nodiscard]] constexpr bool IsLiquid() const noexcept {
    return GetBlockProperties(type).isLiquid;
  }

  [[nodiscard]] constexpr bool IsAir() const noexcept {
    return type == BlockType::Air;
  }
};

} // namespace mc
