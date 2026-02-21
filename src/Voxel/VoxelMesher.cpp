#include "Mamancraft/Voxel/VoxelMesher.hpp"
#include "Mamancraft/Voxel/BlockRegistry.hpp"

namespace mc {

VulkanMesh::Builder VoxelMesher::GenerateMesh(const Chunk &chunk) {
  VulkanMesh::Builder builder;
  const glm::vec3 chunkPosOffset = glm::vec3(chunk.GetPosition() * Chunk::SIZE);

  for (int y = 0; y < Chunk::SIZE; ++y) {
    for (int z = 0; z < Chunk::SIZE; ++z) {
      for (int x = 0; x < Chunk::SIZE; ++x) {
        Block block = chunk.GetBlock(x, y, z);
        if (block.type == BlockType::Air) {
          continue;
        }

        const auto &info = BlockRegistry::GetInfo(block.type);
        const glm::ivec3 blockPos{x, y, z};

        // Top
        if (auto b = chunk.GetBlock(x, y + 1, z);
            b.type == BlockType::Air ||
            BlockRegistry::GetInfo(b.type).isTransparent) {
          AddFace(builder, chunkPosOffset, blockPos, Face::Top, info.color);
        }
        // Bottom
        if (auto b = chunk.GetBlock(x, y - 1, z);
            b.type == BlockType::Air ||
            BlockRegistry::GetInfo(b.type).isTransparent) {
          AddFace(builder, chunkPosOffset, blockPos, Face::Bottom, info.color);
        }
        // Front
        if (auto b = chunk.GetBlock(x, y, z + 1);
            b.type == BlockType::Air ||
            BlockRegistry::GetInfo(b.type).isTransparent) {
          AddFace(builder, chunkPosOffset, blockPos, Face::Front, info.color);
        }
        // Back
        if (auto b = chunk.GetBlock(x, y, z - 1);
            b.type == BlockType::Air ||
            BlockRegistry::GetInfo(b.type).isTransparent) {
          AddFace(builder, chunkPosOffset, blockPos, Face::Back, info.color);
        }
        // Right
        if (auto b = chunk.GetBlock(x + 1, y, z);
            b.type == BlockType::Air ||
            BlockRegistry::GetInfo(b.type).isTransparent) {
          AddFace(builder, chunkPosOffset, blockPos, Face::Right, info.color);
        }
        // Left
        if (auto b = chunk.GetBlock(x - 1, y, z);
            b.type == BlockType::Air ||
            BlockRegistry::GetInfo(b.type).isTransparent) {
          AddFace(builder, chunkPosOffset, blockPos, Face::Left, info.color);
        }
      }
    }
  }

  return builder;
}

void VoxelMesher::AddFace(VulkanMesh::Builder &builder,
                          const glm::vec3 &chunkOffset, const glm::ivec3 &p,
                          Face face, const glm::vec3 &color) {
  const uint32_t startIndex = static_cast<uint32_t>(builder.vertices.size());
  const glm::vec3 bp = glm::vec3(p) + chunkOffset;

  switch (face) {
  case Face::Top:
    builder.vertices.push_back({bp + glm::vec3(0, 1, 0), color});
    builder.vertices.push_back({bp + glm::vec3(0, 1, 1), color});
    builder.vertices.push_back({bp + glm::vec3(1, 1, 1), color});
    builder.vertices.push_back({bp + glm::vec3(1, 1, 0), color});
    break;
  case Face::Bottom:
    builder.vertices.push_back({bp + glm::vec3(0, 0, 0), color});
    builder.vertices.push_back({bp + glm::vec3(1, 0, 0), color});
    builder.vertices.push_back({bp + glm::vec3(1, 0, 1), color});
    builder.vertices.push_back({bp + glm::vec3(0, 0, 1), color});
    break;
  case Face::Front:
    builder.vertices.push_back({bp + glm::vec3(0, 0, 1), color});
    builder.vertices.push_back({bp + glm::vec3(1, 0, 1), color});
    builder.vertices.push_back({bp + glm::vec3(1, 1, 1), color});
    builder.vertices.push_back({bp + glm::vec3(0, 1, 1), color});
    break;
  case Face::Back:
    builder.vertices.push_back({bp + glm::vec3(0, 0, 0), color});
    builder.vertices.push_back({bp + glm::vec3(0, 1, 0), color});
    builder.vertices.push_back({bp + glm::vec3(1, 1, 0), color});
    builder.vertices.push_back({bp + glm::vec3(1, 0, 0), color});
    break;
  case Face::Right:
    builder.vertices.push_back({bp + glm::vec3(1, 0, 0), color});
    builder.vertices.push_back({bp + glm::vec3(1, 1, 0), color});
    builder.vertices.push_back({bp + glm::vec3(1, 1, 1), color});
    builder.vertices.push_back({bp + glm::vec3(1, 0, 1), color});
    break;
  case Face::Left:
    builder.vertices.push_back({bp + glm::vec3(0, 0, 0), color});
    builder.vertices.push_back({bp + glm::vec3(0, 0, 1), color});
    builder.vertices.push_back({bp + glm::vec3(0, 1, 1), color});
    builder.vertices.push_back({bp + glm::vec3(0, 1, 0), color});
    break;
  }

  builder.indices.push_back(startIndex + 0);
  builder.indices.push_back(startIndex + 1);
  builder.indices.push_back(startIndex + 2);
  builder.indices.push_back(startIndex + 2);
  builder.indices.push_back(startIndex + 3);
  builder.indices.push_back(startIndex + 0);
}

} // namespace mc
