#pragma once
#include "Mamancraft/Voxel/Block.hpp"
#include <array>
#include <glm/glm.hpp>
#include <memory>

namespace mc {

class Chunk {
public:
  static constexpr int SIZE = 32;
  static constexpr int VOLUME = SIZE * SIZE * SIZE;

  explicit Chunk(const glm::ivec3 &position) : m_Position(position) {
    m_Blocks.fill({BlockType::Air});
  }

  void SetBlock(int x, int y, int z, Block block) {
    if (IsValid(x, y, z)) {
      m_Blocks[GetIndex(x, y, z)] = block;
    }
  }

  [[nodiscard]] Block GetBlock(int x, int y, int z) const noexcept {
    if (!IsValid(x, y, z)) {
      return {BlockType::Air};
    }
    return m_Blocks[GetIndex(x, y, z)];
  }

  [[nodiscard]] const glm::ivec3 &GetPosition() const noexcept {
    return m_Position;
  }

  [[nodiscard]] static constexpr bool IsValid(int x, int y, int z) noexcept {
    return x >= 0 && x < SIZE && y >= 0 && y < SIZE && z >= 0 && z < SIZE;
  }

private:
  [[nodiscard]] static constexpr int GetIndex(int x, int y, int z) noexcept {
    return x + (y * SIZE) + (z * SIZE * SIZE);
  }

  glm::ivec3 m_Position;
  std::array<Block, VOLUME> m_Blocks{};
};

} // namespace mc
