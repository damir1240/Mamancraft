#include "Mamancraft/Voxel/VoxelMesher.hpp"
#include "Mamancraft/Voxel/BlockRegistry.hpp"

namespace mc {

VulkanMesh::Builder VoxelMesher::GenerateMesh(const Chunk &chunk) {
  VulkanMesh::Builder builder;
  const glm::vec3 chunkPosOffset = glm::vec3(chunk.GetPosition() * Chunk::SIZE);
  const auto &registry = BlockRegistry::Instance();

  // For each of the 6 directions
  for (int d = 0; d < 6; d++) {
    Face face = static_cast<Face>(d);

    // Axis: 0 for X, 1 for Y, 2 for Z
    int axis =
        (d < 2) ? 1
                : (d < 4 ? 2 : 0); // Top/Bottom=Y, Front/Back=Z, Right/Left=X
    int direction = (d % 2 == 0) ? 1 : -1; // Top=1, Bottom=-1, etc.

    // Perpendicular axes
    int axis1 = (axis + 1) % 3;
    int axis2 = (axis + 2) % 3;

    std::vector<VoxelMesher::FaceData> mask(Chunk::SIZE * Chunk::SIZE);

    for (int i = 0; i < Chunk::SIZE; i++) { // Slice along the main axis
      // 1. Fill mask for this slice
      for (int j = 0; j < Chunk::SIZE; j++) {
        for (int k = 0; k < Chunk::SIZE; k++) {
          glm::ivec3 p(0);
          p[axis] = i;
          p[axis1] = j;
          p[axis2] = k;

          Block block = chunk.GetBlock(p.x, p.y, p.z);
          if (block.type == BlockType::Air)
            continue;

          glm::ivec3 neighborP = p;
          neighborP[axis] += direction;

          bool isVisible = false;
          if (neighborP[axis] < 0 || neighborP[axis] >= Chunk::SIZE) {
            isVisible = true; // For now, treat boundaries as visible (later
                              // check neighboring chunks)
          } else {
            Block neighbor =
                chunk.GetBlock(neighborP.x, neighborP.y, neighborP.z);
            if (neighbor.type == BlockType::Air ||
                registry.GetInfo(neighbor.type).isTransparent) {
              isVisible = true;
            }
          }

          if (isVisible) {
            const auto &info = registry.GetInfo(block.type);
            VoxelMesher::FaceData data;
            data.active = true;
            data.color = info.color;
            if (face == Face::Top)
              data.texIndex = info.texIndexTop;
            else if (face == Face::Bottom)
              data.texIndex = info.texIndexBottom;
            else
              data.texIndex = info.texIndexSide;

            mask[j * Chunk::SIZE + k] = data;
          } else {
            mask[j * Chunk::SIZE + k] = {0, {0, 0, 0}, false};
          }
        }
      }

      // 2. Mesh the mask
      for (int j = 0; j < Chunk::SIZE; j++) {
        for (int k = 0; k < Chunk::SIZE; k++) {
          int index = j * Chunk::SIZE + k;
          if (!mask[index].active)
            continue;

          VoxelMesher::FaceData currentData = mask[index];
          int width = 1;
          int height = 1;

          // Expand width
          for (int w = k + 1; w < Chunk::SIZE; w++) {
            if (mask[j * Chunk::SIZE + w] == currentData)
              width++;
            else
              break;
          }

          // Expand height
          for (int h = j + 1; h < Chunk::SIZE; h++) {
            bool rowMatch = true;
            for (int w = 0; w < width; w++) {
              if (!(mask[h * Chunk::SIZE + k + w] == currentData)) {
                rowMatch = false;
                break;
              }
            }
            if (rowMatch)
              height++;
            else
              break;
          }

          // Generate quad
          glm::ivec3 p(0);
          p[axis] = i;
          p[axis1] = j;
          p[axis2] = k;

          AddGreedyFace(builder, chunkPosOffset, p, axis, axis1, axis2,
                        direction, width, height, currentData);

          // Mark as processed
          for (int h = 0; h < height; h++) {
            for (int w = 0; w < width; w++) {
              mask[(j + h) * Chunk::SIZE + (k + w)].active = false;
            }
          }
        }
      }
    }
  }

  return builder;
}

void VoxelMesher::AddGreedyFace(VulkanMesh::Builder &builder,
                                const glm::vec3 &chunkOffset,
                                const glm::ivec3 &p, int axis, int axis1,
                                int axis2, int direction, int width, int height,
                                const VoxelMesher::FaceData &data) {
  const uint32_t startIndex = static_cast<uint32_t>(builder.vertices.size());
  glm::vec3 bp = glm::vec3(p) + chunkOffset;
  if (direction > 0)
    bp[axis] += 1.0f;

  glm::vec3 v1(0), v2(0), v3(0), v4(0);
  v1[axis] = 0;
  v1[axis1] = 0;
  v1[axis2] = 0;
  v2[axis] = 0;
  v2[axis1] = height;
  v2[axis2] = 0;
  v3[axis] = 0;
  v3[axis1] = height;
  v3[axis2] = width;
  v4[axis] = 0;
  v4[axis1] = 0;
  v4[axis2] = width;

  // Normalizing UVs: width and height are the texture repeat factors
  builder.vertices.push_back(
      {bp + v1, data.color, {0.0f, 0.0f}, data.texIndex});
  builder.vertices.push_back(
      {bp + v2, data.color, {0.0f, (float)height}, data.texIndex});
  builder.vertices.push_back(
      {bp + v3, data.color, {(float)width, (float)height}, data.texIndex});
  builder.vertices.push_back(
      {bp + v4, data.color, {(float)width, 0.0f}, data.texIndex});

  // Handle winding order based on direction and axis
  // This is a bit tricky, but for simplicity let's use fixed orders for now
  if ((axis == 1 && direction > 0) || (axis == 0 && direction < 0) ||
      (axis == 2 && direction > 0)) {
    builder.indices.push_back(startIndex + 0);
    builder.indices.push_back(startIndex + 1);
    builder.indices.push_back(startIndex + 2);
    builder.indices.push_back(startIndex + 2);
    builder.indices.push_back(startIndex + 3);
    builder.indices.push_back(startIndex + 0);
  } else {
    builder.indices.push_back(startIndex + 0);
    builder.indices.push_back(startIndex + 3);
    builder.indices.push_back(startIndex + 2);
    builder.indices.push_back(startIndex + 2);
    builder.indices.push_back(startIndex + 1);
    builder.indices.push_back(startIndex + 0);
  }
}

} // namespace mc
