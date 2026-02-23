#include "Mamancraft/Voxel/World.hpp"
#include "Mamancraft/Core/Logger.hpp"
#include "Mamancraft/Voxel/VoxelMesher.hpp"
#include <algorithm>

namespace mc {

World::World(std::unique_ptr<TerrainGenerator> generator,
             TaskSystem &taskSystem)
    : m_Generator(std::move(generator)), m_TaskSystem(taskSystem) {}

void World::Update(const glm::vec3 &playerPos) {
  glm::ivec3 centerChunk =
      glm::ivec3(glm::floor(playerPos / static_cast<float>(Chunk::SIZE)));

  int radius = m_ViewDistance;

  // Build request list with a single pass
  std::vector<glm::ivec3> toRequest;

  {
    std::shared_lock worldLock(m_WorldMutex);
    std::lock_guard loadingLock(m_LoadingMutex);

    for (int x = -radius; x <= radius; x++) {
      for (int z = -radius; z <= radius; z++) {
        // Circular distance check (2D, squared)
        if (x * x + z * z > radius * radius)
          continue;

        // Fixed Y range: terrain lives in chunks Y=0..3 (blocks 0-127)
        // This covers heights 0-127, matching Minecraft-style terrain (base
        // ~64)
        for (int y = 0; y <= 3; y++) {
          glm::ivec3 pos = glm::ivec3(centerChunk.x + x, y, centerChunk.z + z);

          if (m_Chunks.contains(pos) || m_LoadingChunks.contains(pos))
            continue;

          toRequest.push_back(pos);
        }
      }
    }
  }

  // Sort by 3D distance, prioritizing player's Y level
  // Best Practice: Weight Y distance higher so surface chunks load first
  std::sort(toRequest.begin(), toRequest.end(),
            [&centerChunk](const glm::ivec3 &a, const glm::ivec3 &b) {
              glm::ivec3 da = a - centerChunk;
              glm::ivec3 db = b - centerChunk;
              // Weighted: horizontal distance + Y distance * 2
              int distA = da.x * da.x + da.z * da.z + std::abs(da.y) * 4;
              int distB = db.x * db.x + db.z * db.z + std::abs(db.y) * 4;
              return distA < distB;
            });

  // Best Practice: Limit submissions per frame to avoid queue flooding.
  // This keeps the main thread responsive and ensures the closest chunks
  // are processed first by the thread pool.
  constexpr size_t MAX_SUBMISSIONS_PER_FRAME = 32;
  size_t count = std::min(toRequest.size(), MAX_SUBMISSIONS_PER_FRAME);
  for (size_t i = 0; i < count; i++) {
    RequestChunk(toRequest[i]);
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
