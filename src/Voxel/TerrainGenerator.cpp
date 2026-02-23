#include "Mamancraft/Voxel/TerrainGenerator.hpp"
#include <cmath>

namespace mc {

void WaveTerrainGenerator::Generate(Chunk &chunk) const {
  const glm::ivec3 chunkPos = chunk.GetPosition();

  for (int x = 0; x < Chunk::SIZE; x++) {
    for (int z = 0; z < Chunk::SIZE; z++) {
      // Absolute world coordinates for the noise/wave function
      float worldX = static_cast<float>(chunkPos.x * Chunk::SIZE + x);
      float worldZ = static_cast<float>(chunkPos.z * Chunk::SIZE + z);

      int height = static_cast<int>(5.0f + 3.0f * std::sin(worldX * 0.2f) *
                                               std::cos(worldZ * 0.2f));

      // Adjust height relative to chunk Y
      int localHeight = height - (chunkPos.y * Chunk::SIZE);

      for (int y = 0; y < Chunk::SIZE; y++) {
        int worldY = chunkPos.y * Chunk::SIZE + y;

        if (worldY < height) {
          BlockType type = BlockType::Stone;
          if (worldY == height - 1)
            type = BlockType::Grass;
          else if (worldY > height - 4)
            type = BlockType::Dirt;

          chunk.SetBlock(x, y, z, {type});
        } else {
          chunk.SetBlock(x, y, z, {BlockType::Air});
        }
      }
    }
  }
}

} // namespace mc
