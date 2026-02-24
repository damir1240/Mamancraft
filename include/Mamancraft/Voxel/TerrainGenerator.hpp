#pragma once

#include "Mamancraft/Voxel/Biome.hpp"
#include "Mamancraft/Voxel/Chunk.hpp"
#include "Mamancraft/Voxel/HeightmapSampler.hpp"
#include "Mamancraft/Voxel/NoiseConfig.hpp"
#include <cstdint>
#include <span>

namespace mc {

// ============================================================================
// VoxelDecorator — C++20 Concept для плагинов декорирования.
//
// Best Practice (отчёт §«Ranges и Строгие Концепты»):
//   Статический полиморфизм через Concepts вместо virtual dispatch.
//   Нет vtable, нет dynamic_cast — гарантия zero-cost abstraction.
//
// Любой класс, удовлетворяющий этому Concept, может быть подан
// как декоратор в AdvancedTerrainGenerator без изменения его кода.
//
// Требования к типу T:
//   - T::decorate(Chunk&, const ColumnSample&, int localX, int localZ)
//     Размещает декорации (деревья, травинки, руды) для одной колонки.
// ============================================================================

template <typename T>
concept VoxelDecorator = requires(
    T decorator, Chunk &chunk, const ColumnSample &sample, int localX,
    int localZ, int chunkWorldX, int chunkWorldZ) {
  {
    decorator.Decorate(chunk, sample, localX, localZ, chunkWorldX, chunkWorldZ)
  } -> std::same_as<void>;
};

// ============================================================================
// TerrainGenerator — абстрактный базовый класс.
//
// Контракт: Generate() принимает готовый Chunk и заполняет его блоками.
// Намеренно минимален — вся логика в конкретных реализациях.
// ============================================================================

class TerrainGenerator {
public:
  virtual ~TerrainGenerator() = default;

  /// @brief Заполнить чанк данными о блоках.
  virtual void Generate(Chunk &chunk) const = 0;
};

// ============================================================================
// AdvancedTerrainGenerator — основной генератор мира.
//
// Архитектура (Best Practice 2026):
//
//   1. HeightmapSampler (отдельный модуль) считает климат + высоту.
//      Переиспользуется в декораторах без пересчёта шумов.
//
//   2. Двухпроходная генерация:
//      a) Terrain Pass  — заполнение блоков по ColumnSample.
//      b) Decoration Pass — деревья с neighbor-aware сканированием.
//
//   3. Column-first порядок обхода: for(x) for(z) for(y).
//      Лучше cache locality для heightmap[x][z] и блоков.
//
//   4. Река — убрана (согласно требованию рефактора).
//      Зарезервировано место для будущей гидрологии
//      через отдельный декоратор/слой.
//
//   5. Все магические числа — в NoiseConfig.hpp::world_cfg.
// ============================================================================

class AdvancedTerrainGenerator : public TerrainGenerator {
public:
  explicit AdvancedTerrainGenerator(uint32_t seed = 1337u);

  void Generate(Chunk &chunk) const override;

  /// @brief Прямой доступ к сэмплеру (для тестов и внешних декораторов).
  [[nodiscard]] const HeightmapSampler &GetSampler() const noexcept {
    return m_Sampler;
  }

  [[nodiscard]] uint32_t GetSeed() const noexcept { return m_Seed; }

private:
  uint32_t m_Seed;
  HeightmapSampler m_Sampler;

  // ── Terrain Pass helpers ─────────────────────────────────────────────────

  /// Определить тип поверхностного блока для данного биома и высоты.
  [[nodiscard]] BlockType GetSurfaceBlock(const BiomeDefinition &def,
                                          int worldY,
                                          int terrainHeight) const noexcept;

  // ── Decoration Pass helpers ──────────────────────────────────────────────

  /// Deterministic jittered-grid проверка: нужно ли ставить дерево здесь?
  [[nodiscard]] bool ShouldPlaceTree(int worldX, int worldZ,
                                     const BiomeDefinition &def) const noexcept;

  /// Cave placeholder (всегда false — Task: заменить на 3D-шум Перлина).
  [[nodiscard]] bool HasCaveAt(float x, float y, float z) const noexcept;
};

// ============================================================================
// WaveTerrainGenerator — legacy-генератор синусоидального рельефа.
// Оставлен для быстрого smoke-теста рендера без сложной генерации.
// ============================================================================

class WaveTerrainGenerator : public TerrainGenerator {
public:
  void Generate(Chunk &chunk) const override;
};

} // namespace mc
