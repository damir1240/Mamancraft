#include "Mamancraft/Voxel/VoxelMesher.hpp"
#include "Mamancraft/Voxel/BlockRegistry.hpp"

namespace mc {

VulkanMesh::Builder VoxelMesher::GenerateMesh(const Chunk &chunk) {
  VulkanMesh::Builder builder;
  const glm::vec3 chunkPosOffset = glm::vec3(chunk.GetPosition() * Chunk::SIZE);
  const auto &registry = BlockRegistry::Instance();

  for (int y = 0; y < Chunk::SIZE; ++y) {
    for (int z = 0; z < Chunk::SIZE; ++z) {
      for (int x = 0; x < Chunk::SIZE; ++x) {
        Block block = chunk.GetBlock(x, y, z);
        if (block.type == BlockType::Air) {
          continue;
        }

        const auto &info = registry.GetInfo(block.type);
        const glm::ivec3 blockPos{x, y, z};

        // Top (+Y)
        if (auto b = chunk.GetBlock(x, y + 1, z);
            b.type == BlockType::Air ||
            registry.GetInfo(b.type).isTransparent) {
          AddFace(builder, chunkPosOffset, blockPos, Face::Top, info);
        }
        // Bottom (-Y)
        if (auto b = chunk.GetBlock(x, y - 1, z);
            b.type == BlockType::Air ||
            registry.GetInfo(b.type).isTransparent) {
          AddFace(builder, chunkPosOffset, blockPos, Face::Bottom, info);
        }
        // Front (+Z)
        if (auto b = chunk.GetBlock(x, y, z + 1);
            b.type == BlockType::Air ||
            registry.GetInfo(b.type).isTransparent) {
          AddFace(builder, chunkPosOffset, blockPos, Face::Front, info);
        }
        // Back (-Z)
        if (auto b = chunk.GetBlock(x, y, z - 1);
            b.type == BlockType::Air ||
            registry.GetInfo(b.type).isTransparent) {
          AddFace(builder, chunkPosOffset, blockPos, Face::Back, info);
        }
        // Right (+X)
        if (auto b = chunk.GetBlock(x + 1, y, z);
            b.type == BlockType::Air ||
            registry.GetInfo(b.type).isTransparent) {
          AddFace(builder, chunkPosOffset, blockPos, Face::Right, info);
        }
        // Left (-X)
        if (auto b = chunk.GetBlock(x - 1, y, z);
            b.type == BlockType::Air ||
            registry.GetInfo(b.type).isTransparent) {
          AddFace(builder, chunkPosOffset, blockPos, Face::Left, info);
        }
      }
    }
  }

  return builder;
}

void VoxelMesher::AddFace(VulkanMesh::Builder &builder,
                          const glm::vec3 &chunkOffset, const glm::ivec3 &p,
                          Face face, const BlockInfo &info) {
  const uint32_t startIndex = static_cast<uint32_t>(builder.vertices.size());
  const glm::vec3 bp = glm::vec3(p) + chunkOffset;
  const glm::vec3 color = info.color;
  uint32_t texIdx = 0;

  switch (face) {
  case Face::Top:
    texIdx = info.texIndexTop;
    break;
  case Face::Bottom:
    texIdx = info.texIndexBottom;
    break;
  default:
    texIdx = info.texIndexSide;
    break;
  }

  switch (face) {
  case Face::Top:
    builder.vertices.push_back(
        {bp + glm::vec3(0, 1, 0), color, {0, 0}, texIdx});
    builder.vertices.push_back(
        {bp + glm::vec3(0, 1, 1), color, {0, 1}, texIdx});
    builder.vertices.push_back(
        {bp + glm::vec3(1, 1, 1), color, {1, 1}, texIdx});
    builder.vertices.push_back(
        {bp + glm::vec3(1, 1, 0), color, {1, 0}, texIdx});
    break;
  case Face::Bottom:
    builder.vertices.push_back(
        {bp + glm::vec3(0, 0, 0), color, {0, 0}, texIdx});
    builder.vertices.push_back(
        {bp + glm::vec3(1, 0, 0), color, {1, 0}, texIdx});
    builder.vertices.push_back(
        {bp + glm::vec3(1, 0, 1), color, {1, 1}, texIdx});
    builder.vertices.push_back(
        {bp + glm::vec3(0, 0, 1), color, {0, 1}, texIdx});
    break;
  case Face::Front:
    builder.vertices.push_back(
        {bp + glm::vec3(0, 0, 1), color, {0, 1}, texIdx});
    builder.vertices.push_back(
        {bp + glm::vec3(1, 0, 1), color, {1, 1}, texIdx});
    builder.vertices.push_back(
        {bp + glm::vec3(1, 1, 1), color, {1, 0}, texIdx});
    builder.vertices.push_back(
        {bp + glm::vec3(0, 1, 1), color, {0, 0}, texIdx});
    break;
  case Face::Back:
    builder.vertices.push_back(
        {bp + glm::vec3(0, 0, 0), color, {1, 1}, texIdx});
    builder.vertices.push_back(
        {bp + glm::vec3(0, 1, 0), color, {1, 0}, texIdx});
    builder.vertices.push_back(
        {bp + glm::vec3(1, 1, 0), color, {0, 0}, texIdx});
    builder.vertices.push_back(
        {bp + glm::vec3(1, 0, 0), color, {0, 1}, texIdx});
    break;
  case Face::Right:
    builder.vertices.push_back(
        {bp + glm::vec3(1, 0, 0), color, {1, 1}, texIdx});
    builder.vertices.push_back(
        {bp + glm::vec3(1, 1, 0), color, {1, 0}, texIdx});
    builder.vertices.push_back(
        {bp + glm::vec3(1, 1, 1), color, {0, 0}, texIdx});
    builder.vertices.push_back(
        {bp + glm::vec3(1, 0, 1), color, {0, 1}, texIdx});
    break;
  case Face::Left:
    builder.vertices.push_back(
        {bp + glm::vec3(0, 0, 0), color, {0, 1}, texIdx});
    builder.vertices.push_back(
        {bp + glm::vec3(0, 0, 1), color, {1, 1}, texIdx});
    builder.vertices.push_back(
        {bp + glm::vec3(0, 1, 1), color, {1, 0}, texIdx});
    builder.vertices.push_back(
        {bp + glm::vec3(0, 1, 0), color, {0, 0}, texIdx});
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
