#pragma once
#include <cstdint>

namespace mc {

enum class BlockType : uint8_t {
  Air = 0,
  Dirt,
  Grass,
  Stone,
  Wood,
  Leaves,
  Bedrock,
  Water,
  Count
};

struct Block {
  BlockType type = BlockType::Air;

  [[nodiscard]] constexpr bool IsOpaque() const noexcept {
    return type != BlockType::Air && type != BlockType::Water;
  }

  [[nodiscard]] constexpr bool IsSolid() const noexcept {
    // Water is not solid — entities/player can pass through it.
    return type != BlockType::Air && type != BlockType::Water;
  }

  [[nodiscard]] constexpr bool IsLiquid() const noexcept {
    return type == BlockType::Water;
  }
};

} // namespace mc
