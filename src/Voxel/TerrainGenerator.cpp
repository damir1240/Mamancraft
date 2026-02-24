#include "Mamancraft/Voxel/TerrainGenerator.hpp"
#include "Mamancraft/Voxel/NoiseConfig.hpp"
#include "Mamancraft/Voxel/OakTreeGenerator.hpp"
#include <algorithm>
#include <cmath>

namespace mc {

// ============================================================================
// AdvancedTerrainGenerator
// ============================================================================

AdvancedTerrainGenerator::AdvancedTerrainGenerator(uint32_t seed)
    : m_Seed(seed), m_Sampler(seed) {}

// ============================================================================
// GetSurfaceBlock — data-driven, читает параметры из BiomeDefinition.
//
// Best Practice: нет switch/case по BiomeType.
// Логика «когда наступает камень» обобщена через stoneStartFactor.
// ============================================================================

BlockType AdvancedTerrainGenerator::GetSurfaceBlock(
    const BiomeDefinition &def, int worldY, int terrainHeight) const noexcept {
  using namespace world_cfg;

  // Горный биом: камень появляется в верхней части хребта.
  // stoneStartFactor == 999 → камень на поверхности никогда не появляется.
  if (def.stoneStartFactor < 999) {
    // Камень стартует с высоты BASE + (amplitude * factor / 100)
    float stoneStart =
        BASE_HEIGHT + (terrainHeight - BASE_HEIGHT) *
                          (static_cast<float>(def.stoneStartFactor) / 100.0f);
    // Жёсткий минимум: никогда ниже BASE + 20 блоков (предотвращает
    // каменные холмы у подножий)
    stoneStart = std::max(stoneStart, BASE_HEIGHT + 20.0f);
    if (worldY >= static_cast<int>(stoneStart)) {
      return BlockType::Stone;
    }
  }

  return def.surfaceBlock;
}

// ============================================================================
// ShouldPlaceTree — детерминированная расстановка через Jittered Grid.
//
// Best Practice: одно дерево на ячейку CELL_SIZE×CELL_SIZE,
// позиция внутри ячейки задаётся хэш-джиттером.
// Результат: деревья не стоят в ряд, но и не образуют кластеры.
// ============================================================================

bool AdvancedTerrainGenerator::ShouldPlaceTree(
    int worldX, int worldZ, const BiomeDefinition &def) const noexcept {

  if (def.treeDensity <= 0.0f)
    return false;

  using namespace world_cfg;

  // Ячейка, к которой принадлежит эта мировая позиция
  // (корректная целочисленная div для отрицательных координат)
  auto floorDiv = [](int a, int b) -> int {
    return a / b - (a % b != 0 && (a ^ b) < 0);
  };

  int cellX = floorDiv(worldX, TREE_CELL_SIZE);
  int cellZ = floorDiv(worldZ, TREE_CELL_SIZE);

  // Детерминированный хэш ячейки (FNV-like, как в ранней версии)
  uint32_t h = m_Seed * 16777619u;
  h ^= static_cast<uint32_t>(cellX) * 374761393u;
  h ^= static_cast<uint32_t>(cellZ) * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  h ^= h >> 16;

  // Вероятностный отбор: масштабируем плотность на площадь ячейки
  float chance = static_cast<float>(h & 0xFFFFu) / 65536.0f;
  if (chance > def.treeDensity * static_cast<float>(TREE_CELL_SIZE)) {
    return false;
  }

  // Джиттер: случайное смещение центра ячейки в диапазоне −3..+4
  int cellBaseX = cellX * TREE_CELL_SIZE;
  int cellBaseZ = cellZ * TREE_CELL_SIZE;
  int jitterX = static_cast<int>((h >> 4) & 0x7u) - 3;
  int jitterZ = static_cast<int>((h >> 8) & 0x7u) - 3;
  int treeX = cellBaseX + TREE_CELL_SIZE / 2 + jitterX;
  int treeZ = cellBaseZ + TREE_CELL_SIZE / 2 + jitterZ;

  return (worldX == treeX && worldZ == treeZ);
}

// ============================================================================
// HasCaveAt — заглушка для будущих 3D-пещер.
// Task: заменить на Perlin Worms или Simplex 3D Cave noise.
// ============================================================================

bool AdvancedTerrainGenerator::HasCaveAt(float /*x*/, float /*y*/,
                                         float /*z*/) const noexcept {
  return false;
}

// ============================================================================
// Generate — главный метод. Двухпроходная архитектура:
//
//   Pass 1 — Terrain Pass
//     Для каждой колонки (x, z):
//       a) HeightmapSampler::Sample() → ColumnSample (климат, высота, биом)
//       b) Заполнить вертикальный столбец блоками согласно ColumnSample
//     Кэшируем ColumnSample в columns[][] для Pass 2.
//
//   Pass 2 — Decoration Pass (Neighbor-Aware)
//     Сканируем регион CHUNK + SCAN_MARGIN со всех сторон.
//     Для позиций внутри чанка — берём данные из кэша.
//     Для позиций снаружи — вызываем HeightmapSampler::Sample() повторно.
//     Размещаем деревья, которые физически попадают в этот чанк.
//
// Ранние выходы (early-exit):
//   — Чанк полностью выше MAX_HEIGHT → весь воздух, return.
//   — Чанк полностью ниже MIN_HEIGHT → всё камень/бедрок, return.
// ============================================================================

void AdvancedTerrainGenerator::Generate(Chunk &chunk) const {
  using namespace world_cfg;

  const glm::ivec3 chunkPos = chunk.GetPosition();
  const int chunkBottomY = chunkPos.y * Chunk::SIZE;
  const int chunkTopY = chunkBottomY + Chunk::SIZE - 1;

  // ── Early Exit: чанк выше всего рельефа → оставляем воздух ───────────────
  if (chunkBottomY > MAX_HEIGHT)
    return;

  // ── Early Exit: чанк ниже минимального рельефа → всё каменное/бедровое ───
  if (chunkTopY < MIN_HEIGHT) {
    for (int x = 0; x < Chunk::SIZE; ++x) {
      for (int z = 0; z < Chunk::SIZE; ++z) {
        for (int y = 0; y < Chunk::SIZE; ++y) {
          const int worldY = chunkBottomY + y;
          chunk.SetBlock(
              x, y, z,
              {worldY < BEDROCK_MAX ? BlockType::Bedrock : BlockType::Stone});
        }
      }
    }
    return;
  }

  // ── Кэш результатов сэмплирования для Pass 2 ─────────────────────────────
  ColumnSample columns[Chunk::SIZE][Chunk::SIZE];

  // ==========================================================================
  // PASS 1 — TERRAIN
  // Порядок x → z → y обеспечивает наилучшую cache-locality:
  //   heightmap читается построчно, вертикальный стек блоков обходится
  //   последовательно по памяти (layout: x + y*SIZE + z*SIZE*SIZE).
  // ==========================================================================

  const int chunkWorldX = chunkPos.x * Chunk::SIZE;
  const int chunkWorldZ = chunkPos.z * Chunk::SIZE;

  for (int x = 0; x < Chunk::SIZE; ++x) {
    for (int z = 0; z < Chunk::SIZE; ++z) {
      const float worldX = static_cast<float>(chunkWorldX + x);
      const float worldZ = static_cast<float>(chunkWorldZ + z);

      // Получаем всё о колонке за один вызов (климат + биом + высота)
      const ColumnSample sample = m_Sampler.Sample(worldX, worldZ);
      columns[x][z] = sample;

      const int iHeight = static_cast<int>(sample.height);
      const BiomeDefinition &def = GetBiomeDef(sample.biome);

      // ── Заполнение вертикального столбца ──────────────────────────────────
      for (int y = 0; y < Chunk::SIZE; ++y) {
        const int worldY = chunkBottomY + y;

        if (worldY > iHeight) {
          // Выше поверхности — воздух (Chunk инициализирован Air по умолчанию)
          // chunk.SetBlock(x, y, z, {BlockType::Air}); // избыточно, но явно
          continue;
        }

        // Cave culling (заглушка, будущая фича)
        if (HasCaveAt(worldX, static_cast<float>(worldY), worldZ)) {
          // оставляем Air
          continue;
        }

        BlockType type;

        if (worldY < BEDROCK_MAX) {
          type = BlockType::Bedrock;
        } else if (worldY == iHeight) {
          // Поверхностный блок — определяется биомом и высотой
          type = GetSurfaceBlock(def, worldY, iHeight);
        } else if (worldY > iHeight - def.subSurfaceDepth) {
          // Подповерхностный слой (dirt, sand и т.д.)
          type = def.subSurfaceBlock;
        } else {
          type = BlockType::Stone;
        }

        chunk.SetBlock(x, y, z, {type});
      }
    }
  }

  // ==========================================================================
  // PASS 2 — DECORATION (Neighbor-Aware Trees)
  //
  // Best Practice: сканируем регион больше чанка на TREE_SCAN_MARGIN
  // с каждой стороны. Деревья из соседних чанков могут иметь ветви
  // и листья, которые физически попадают в ЭТОТ чанк.
  //
  // Для внутренних позиций — кэш из Pass 1 (нет повторных шумовых вызовов).
  // Для внешних позиций — HeightmapSampler::Sample() (потокобезопасен).
  // ==========================================================================

  for (int sx = -TREE_SCAN_MARGIN; sx < Chunk::SIZE + TREE_SCAN_MARGIN; ++sx) {
    for (int sz = -TREE_SCAN_MARGIN; sz < Chunk::SIZE + TREE_SCAN_MARGIN;
         ++sz) {

      const int wx = chunkWorldX + sx;
      const int wz = chunkWorldZ + sz;

      // Берём ColumnSample: из кэша (быстро) или пересчитываем (снаружи чанка)
      ColumnSample sample;
      if (sx >= 0 && sx < Chunk::SIZE && sz >= 0 && sz < Chunk::SIZE) {
        sample = columns[sx][sz];
      } else {
        sample =
            m_Sampler.Sample(static_cast<float>(wx), static_cast<float>(wz));
      }

      const BiomeDefinition &def = GetBiomeDef(sample.biome);

      if (!ShouldPlaceTree(wx, wz, def))
        continue;

      // Генерируем блупринт дерева (детерминированно по мировым координатам)
      const auto treeBlocks = OakTreeGenerator::Generate(wx, wz, m_Seed);
      const int treeBaseY = static_cast<int>(sample.height) + 1;

      // Размещаем блоки дерева, попадающие в границы ЭТОГО чанка
      for (const auto &tb : treeBlocks) {
        const int bx = sx + tb.dx;
        const int by = (treeBaseY + tb.dy) - chunkBottomY;
        const int bz = sz + tb.dz;

        if (!Chunk::IsValid(bx, by, bz))
          continue;

        // Правило приоритета: Wood перезаписывает Leaves, Air не трогает ничего
        const Block existing = chunk.GetBlock(bx, by, bz);
        if (existing.IsAir() || (tb.type == BlockType::Wood &&
                                 existing.type == BlockType::Leaves)) {
          chunk.SetBlock(bx, by, bz, {tb.type});
        }
      }
    }
  }
}

// ============================================================================
// WaveTerrainGenerator — простой синусоидальный рельеф для smoke-тестов.
// ============================================================================

void WaveTerrainGenerator::Generate(Chunk &chunk) const {
  const glm::ivec3 chunkPos = chunk.GetPosition();
  const int worldYBase = chunkPos.y * Chunk::SIZE;

  for (int x = 0; x < Chunk::SIZE; ++x) {
    for (int z = 0; z < Chunk::SIZE; ++z) {
      const float fx = static_cast<float>(chunkPos.x * Chunk::SIZE + x);
      const float fz = static_cast<float>(chunkPos.z * Chunk::SIZE + z);

      const int height = static_cast<int>(32.0f + 10.0f * std::sin(fx * 0.1f) *
                                                      std::cos(fz * 0.1f));

      for (int y = 0; y < Chunk::SIZE; ++y) {
        const int worldY = worldYBase + y;

        BlockType type = BlockType::Air;
        if (worldY < height - 3) {
          type = BlockType::Stone;
        } else if (worldY < height) {
          type = BlockType::Dirt;
        } else if (worldY == height) {
          type = BlockType::Grass;
        }

        if (type != BlockType::Air) {
          chunk.SetBlock(x, y, z, {type});
        }
      }
    }
  }
}

} // namespace mc
