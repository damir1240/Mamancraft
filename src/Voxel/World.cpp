#include "Mamancraft/Voxel/World.hpp"
#include "Mamancraft/Core/Logger.hpp"

namespace mc {

World::World(std::unique_ptr<TerrainGenerator> generator)
    : m_Generator(std::move(generator)) {}

std::shared_ptr<Chunk> World::GetChunk(const glm::ivec3 &position) {
  std::shared_lock lock(m_WorldMutex);
  if (auto it = m_Chunks.find(position); it != m_Chunks.end()) {
    return it->second;
  }

  // Upgrade to unique lock for creation
  lock.unlock();
  std::unique_lock ulock(m_WorldMutex);

  // Double check after lock upgrade
  if (auto it = m_Chunks.find(position); it != m_Chunks.end()) {
    return it->second;
  }

  auto chunk = std::make_shared<Chunk>(position);
  m_Generator->Generate(*chunk);
  m_Chunks[position] = chunk;

  return chunk;
}

bool World::HasChunk(const glm::ivec3 &position) const {
  std::shared_lock lock(m_WorldMutex);
  return m_Chunks.contains(position);
}

void World::LoadChunk(const glm::ivec3 &position) {
  GetChunk(position); // Simple implementation for now
}

} // namespace mc
