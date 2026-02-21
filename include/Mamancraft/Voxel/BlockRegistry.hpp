#pragma once
#include "Mamancraft/Voxel/Block.hpp"
#include <glm/glm.hpp>
#include <map>

namespace mc {

struct BlockInfo {
  glm::vec3 color;
  bool isTransparent;
};

class BlockRegistry {
public:
  [[nodiscard]] static const BlockInfo &GetInfo(BlockType type) noexcept {
    static const std::map<BlockType, BlockInfo> registry = {
        {BlockType::Air, {{0.0f, 0.0f, 0.0f}, true}},
        {BlockType::Dirt, {{0.45f, 0.29f, 0.17f}, false}},
        {BlockType::Grass, {{0.27f, 0.52f, 0.19f}, false}},
        {BlockType::Stone, {{0.58f, 0.61f, 0.62f}, false}},
        {BlockType::Wood, {{0.33f, 0.23f, 0.13f}, false}},
        {BlockType::Leaves, {{0.16f, 0.47f, 0.11f}, false}},
        {BlockType::Bedrock, {{0.21f, 0.21f, 0.21f}, false}}};

    if (auto it = registry.find(type); it != registry.end()) {
      return it->second;
    }
    static const BlockInfo unknown = {{1.0f, 0.0f, 1.0f}, false};
    return unknown;
  }
};

} // namespace mc
