#pragma once
#include "Mamancraft/Voxel/Block.hpp"
#include <glm/glm.hpp>
#include <map>
#include <string>

namespace mc {

struct BlockInfo {
  glm::vec3 color;
  bool isTransparent;
  std::string textureTop;
  std::string textureSide;
  std::string textureBottom;

  // Animation (for strip textures like water: Nx1 frames in a vertical strip)
  bool isAnimated = false;
  uint32_t animFrames = 1; // Total number of frames in the strip

  // Runtime indices (populated by AssetManager/Renderer)
  uint32_t texIndexTop = 0;
  uint32_t texIndexSide = 0;
  uint32_t texIndexBottom = 0;
};

class BlockRegistry {
public:
  static BlockRegistry &Instance() {
    static BlockRegistry instance;
    return instance;
  }

  [[nodiscard]] const BlockInfo &GetInfo(BlockType type) const {
    if (auto it = m_Registry.find(type); it != m_Registry.end()) {
      return it->second;
    }
    static const BlockInfo unknown = {{1.0f, 0.0f, 1.0f},
                                      false,
                                      "mc:textures/block/debug.png",
                                      "mc:textures/block/debug.png",
                                      "mc:textures/block/debug.png"};
    return unknown;
  }

  [[nodiscard]] std::map<BlockType, BlockInfo> &GetMutableRegistry() {
    return m_Registry;
  }

private:
  BlockRegistry() {
    m_Registry = {
        {BlockType::Air, {{0.0f, 0.0f, 0.0f}, true, "", "", ""}},

        {BlockType::Dirt,
         {{1.0f, 1.0f, 1.0f},
          false,
          "mc:textures/block/dirt.png",
          "mc:textures/block/dirt.png",
          "mc:textures/block/dirt.png"}},

        {BlockType::Grass,
         {{1.0f, 1.0f, 1.0f},
          false,
          "mc:textures/block/grass_top.png",
          "mc:textures/block/grass_side.png",
          "mc:textures/block/dirt.png"}},

        {BlockType::Stone,
         {{1.0f, 1.0f, 1.0f},
          false,
          "mc:textures/block/stone.png",
          "mc:textures/block/stone.png",
          "mc:textures/block/stone.png"}},

        {BlockType::Wood,
         {{1.0f, 1.0f, 1.0f},
          false,
          "mc:textures/block/oak_log_top.png",
          "mc:textures/block/oak_log.png",
          "mc:textures/block/oak_log_top.png"}},

        {BlockType::Leaves,
         {{1.0f, 1.0f, 1.0f},
          true, // transparent: neighbors ARE rendered through leaves
          "mc:textures/block/oak_leaves.png",
          "mc:textures/block/oak_leaves.png",
          "mc:textures/block/oak_leaves.png"}},

        {BlockType::Bedrock,
         {{1.0f, 1.0f, 1.0f},
          false,
          "mc:textures/block/bedrock.png",
          "mc:textures/block/bedrock.png",
          "mc:textures/block/bedrock.png"}},

        // ── Water ──────────────────────────────────────────────────────────
        // Transparent, animated (16 frames × 16px in a 16×256 vertical strip).
        // The shader uses time + animFrames to cycle through the frames.
        {BlockType::Water,
         {{0.4f, 0.6f, 1.0f}, // tint color (medium blue)
          true,               // transparent: render blocks below water
          "mc:textures/block/water_cauldron.png",
          "mc:textures/block/water_cauldron.png",
          "mc:textures/block/water_cauldron.png",
          /* isAnimated */ true,
          /* animFrames */ 16}},
    };
  }

  std::map<BlockType, BlockInfo> m_Registry;
};

} // namespace mc
