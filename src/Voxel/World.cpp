#include "Mamancraft/Voxel/World.hpp"
#include "Mamancraft/Core/Logger.hpp"
#include "Mamancraft/Voxel/VoxelMesher.hpp"

namespace mc {

World::World(std::unique_ptr<TerrainGenerator> generator,
             TaskSystem &taskSystem)
    : m_Generator(std::move(generator)), m_TaskSystem(taskSystem) {}

void World::Update(const glm::vec3 &playerPos) {
  // Basic implementation: Load a 3x3x3 area around the player if not exists
  glm::ivec3 centerChunk =
      glm::ivec3(glm::floor(playerPos / static_cast<float>(Chunk::SIZE)));

  const int radius = 1; // 3x3x3 for testing
  for (int x = -radius; x <= radius; x++) {
    for (int y = -radius; y <= radius; y++) {
      for (int z = -radius; z <= radius; z++) {
        glm::ivec3 pos = centerChunk + glm::ivec3(x, y, z);
        if (!HasChunk(pos)) {
          RequestChunk(pos);
        }
      }
    }
  }
}

std::vector<std::pair<glm::ivec3, VulkanMesh::Builder>>
World::GetPendingMeshes() {
  std::lock_guard lock(m_PendingMutex);
  std::vector<std::pair<glm::ivec3, VulkanMesh::Builder>> meshes;
  meshes.swap(m_PendingMeshes);
  return meshes;
}

std::shared_ptr<Chunk> World::GetChunk(const glm::ivec3 &position) {
  std::shared_lock lock(m_WorldMutex);
  if (auto it = m_Chunks.find(position); it != m_Chunks.end()) {
    return it->second;
  }
  return nullptr;
}

bool World::HasChunk(const glm::ivec3 &position) const {
  std::shared_lock lock(m_WorldMutex);
  return m_Chunks.contains(position);
}

void World::RequestChunk(const glm::ivec3 &position) {
  {
    std::lock_guard lock(m_LoadingMutex);
    if (m_LoadingChunks.contains(position))
      return;
    m_LoadingChunks.insert(position);
  }

  // Submit task to background threads
  m_TaskSystem.Enqueue([this, position]() {
    // 1. Generate Chunk Data
    auto chunk = std::make_shared<Chunk>(position);
    m_Generator->Generate(*chunk);

    // 2. Add to world map
    {
      std::unique_lock lock(m_WorldMutex);
      m_Chunks[position] = chunk;
    }

    // 3. Generate Mesh (Greedy Meshing)
    VulkanMesh::Builder builder = VoxelMesher::GenerateMesh(*chunk);

    // 4. Send to pending upload queue
    {
      std::lock_guard lock(m_PendingMutex);
      m_PendingMeshes.emplace_back(position, std::move(builder));
    }

    // 5. Mark as loaded
    {
      std::lock_guard lock(m_LoadingMutex);
      m_LoadingChunks.erase(position);
    }

    MC_DEBUG("World: Chunk [{}, {}, {}] ready ({} verts)", position.x,
             position.y, position.z, builder.vertices.size());
  });
}

} // namespace mc
