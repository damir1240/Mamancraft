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

  struct FaceData {
    uint32_t materialID = 0;
    bool active = false;

    bool operator==(const FaceData &other) const {
      return active == other.active && materialID == other.materialID;
    }
  };

  static void AddGreedyFace(VulkanMesh::Builder &builder,
                            const glm::vec3 &chunkOffset, const glm::ivec3 &p,
                            int axis, int axis1, int axis2, int direction,
                            int width, int height, const FaceData &data);
};

} // namespace mc
