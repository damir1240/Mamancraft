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
  Count
};

struct Block {
  BlockType type = BlockType::Air;

  [[nodiscard]] constexpr bool IsOpaque() const noexcept {
    return type != BlockType::Air;
  }

  [[nodiscard]] constexpr bool IsSolid() const noexcept {
    return type != BlockType::Air;
  }
};

} // namespace mc
