#pragma once
#include "Mamancraft/Renderer/Vulkan/VulkanMesh.hpp"
#include "Mamancraft/Voxel/Chunk.hpp"

namespace mc {

class VoxelMesher {
public:
  [[nodiscard]] static VulkanMesh::Builder GenerateMesh(const Chunk &chunk);

private:
  enum class Face {
    Top = 0, // +Y
    Bottom,  // -Y
    Front,   // +Z
    Back,    // -Z
    Right,   // +X
    Left     // -X
  };

  static void AddFace(VulkanMesh::Builder &builder, const glm::vec3 &chunkPos,
                      const glm::ivec3 &blockPos, Face face,
                      const struct BlockInfo &info);
};

} // namespace mc
