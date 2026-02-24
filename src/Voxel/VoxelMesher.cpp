#include "Mamancraft/Voxel/VoxelMesher.hpp"
#include "Mamancraft/Voxel/BlockRegistry.hpp"

namespace mc {

VulkanMesh::Builder VoxelMesher::GenerateMesh(const Chunk &chunk) {
  VulkanMesh::Builder builder;

  // Performance: Quick scan — skip all-air chunks (nothing to draw).
  // NOTE: All-solid chunks MUST still be meshed because boundary blocks
  // have exposed faces (neighboring outside-of-chunk = treated as air).
  bool hasSolid = false;
  for (int i = 0; i < Chunk::VOLUME && !hasSolid; i++) {
    int x = i % Chunk::SIZE;
    int y = (i / Chunk::SIZE) % Chunk::SIZE;
    int z = i / (Chunk::SIZE * Chunk::SIZE);
    if (chunk.GetBlock(x, y, z).type != BlockType::Air)
      hasSolid = true;
  }
  if (!hasSolid)
    return builder;

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
            isVisible = true;
          } else {
            Block neighbor =
                chunk.GetBlock(neighborP.x, neighborP.y, neighborP.z);
            if (neighbor.type == BlockType::Air) {
              isVisible = true;
            } else if (block.type == BlockType::Water &&
                       neighbor.type == BlockType::Water) {
              // Never render shared face between two water blocks
              isVisible = false;
            } else if (!block.IsOpaque() ||
                       registry.GetInfo(neighbor.type).isTransparent) {
              // Transparent block next to another transparent or solid is
              // visible only if neighbor is air (handled above) — except leaves
              // vs solid
              isVisible = !neighbor.IsSolid();
            }
          }

          if (isVisible) {
            const auto &info = registry.GetInfo(block.type);
            VoxelMesher::FaceData data;
            data.active = true;
            data.color = info.color;
            data.animFrames = info.animFrames;
            if (face == Face::Top)
              data.texIndex = info.texIndexTop;
            else if (face == Face::Bottom)
              data.texIndex = info.texIndexBottom;
            else
              data.texIndex = info.texIndexSide;

            mask[j * Chunk::SIZE + k] = data;
          } else {
            mask[j * Chunk::SIZE + k] = {0, 1, {0, 0, 0}, false};
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

  // v1..v4 = 4 corners of the quad, going around:
  //   v1 = (axis1=0, axis2=0)
  //   v2 = (axis1=height, axis2=0)
  //   v3 = (axis1=height, axis2=width)
  //   v4 = (axis1=0, axis2=width)
  glm::vec3 v1(0), v2(0), v3(0), v4(0);
  v2[axis1] = (float)height;
  v3[axis1] = (float)height;
  v3[axis2] = (float)width;
  v4[axis2] = (float)width;

  // UV mapping: U goes along horizontal world-axis, V goes along vertical (Y).
  // axis=1 (Y) => top/bottom face: U=axis2(X or Z), V=axis1(Z or X)
  // axis=0 (X) => right/left face: axis1=Y, axis2=Z  → U=width(Z), V=height(Y)
  // axis=2 (Z) => front/back face: axis1=Y, axis2=X  → U=width(X), V=height(Y)
  //
  // For top/bottom (axis==1): axis1=(X or Z), axis2=(Z or X)
  //   We want U along axis2, V along axis1  →  corners:
  //   v1(0,0), v2(0,h), v3(w,h), v4(w,0)  - this is already correct
  //
  // For side faces (axis==0 or axis==2):
  //   axis1 could be Y or horizontal. Let's check:
  //   axis=0(X): axis1=1(Y), axis2=2(Z) → U=Z(width,k), V=Y(height,j) - correct
  //   axis=2(Z): axis1=0(X), axis2=1(Y) → but axis2=Y means width goes along Y,
  //                                         height goes along X - WRONG!
  //
  // So for axis=2 we need to swap U and V assignment:

  glm::vec2 uv1, uv2, uv3, uv4;

  if (axis == 1) {
    // Top / Bottom: U=axis2, V=axis1
    uv1 = {0.0f, 0.0f};
    uv2 = {0.0f, (float)height};
    uv3 = {(float)width, (float)height};
    uv4 = {(float)width, 0.0f};
  } else if (axis == 0) {
    // Right / Left (X): axis1=Y(vert), axis2=Z(horiz) → U=Z, V=Y
    // j=axis1=Y goes up, k=axis2=Z goes right → U=width(Z), V=height(Y)
    // v1(j=0,k=0), v2(j=h,k=0), v3(j=h,k=w), v4(j=0,k=w)
    // → uv1(0,0), uv2(0,h), uv3(w,h), uv4(w,0)  — U=k, V=j → but V should go 0
    // at top
    uv1 = {0.0f, (float)height};         // j=0 → V=height (bottom of tex)
    uv2 = {0.0f, 0.0f};                  // j=h → V=0      (top of tex)
    uv3 = {(float)width, 0.0f};          // j=h, k=w
    uv4 = {(float)width, (float)height}; // j=0, k=w
  } else {
    // axis == 2: Front / Back (Z): axis1=X(horiz), axis2=Y(vert)
    // j=axis1=X goes right, k=axis2=Y goes up
    // v1(j=0,k=0), v2(j=h,k=0), v3(j=h,k=w), v4(j=0,k=w)
    // → U=j(X), V=k(Y), but V at k=0 is bottom → need to flip V
    uv1 = {0.0f, (float)width};          // k=0 → V=width (bottom of tex)
    uv2 = {(float)height, (float)width}; // j=h, k=0
    uv3 = {(float)height, 0.0f};         // j=h, k=w → V=0 (top)
    uv4 = {0.0f, 0.0f};                  // j=0, k=w
  }

  builder.vertices.push_back(
      {bp + v1, data.color, uv1, data.texIndex, data.animFrames});
  builder.vertices.push_back(
      {bp + v2, data.color, uv2, data.texIndex, data.animFrames});
  builder.vertices.push_back(
      {bp + v3, data.color, uv3, data.texIndex, data.animFrames});
  builder.vertices.push_back(
      {bp + v4, data.color, uv4, data.texIndex, data.animFrames});

  // Winding order: front-face CCW when viewed from outside
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
