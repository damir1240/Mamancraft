#include "Mamancraft/Core/AssetManager.hpp"
#include "Mamancraft/Core/Logger.hpp"
#include "Mamancraft/Renderer/VulkanContext.hpp"

#include <SDL3/SDL.h>

namespace mc {

AssetManager::AssetManager(VulkanContext &context) : m_Context(context) {
  const char *basePath = SDL_GetBasePath();
  if (basePath) {
    m_BaseDir = std::filesystem::path(basePath);
  } else {
    MC_WARN("SDL_GetBasePath failed, using current directory for assets.");
    m_BaseDir = std::filesystem::current_path();
  }

  MC_INFO("AssetManager initialized. Base directory: {}", m_BaseDir.string());
}

AssetManager::~AssetManager() {
  MC_INFO("Shutting down AssetManager (clearing caches)");
  m_ShaderCache.clear();
  m_MeshCache.clear();
}

std::shared_ptr<VulkanShader> AssetManager::GetShader(const std::string &name) {
  if (m_ShaderCache.contains(name)) {
    return m_ShaderCache[name];
  }

  std::filesystem::path fullPath = GetFullPath(name);

  try {
    MC_INFO("Loading shader: {}", fullPath.string());
    auto shader = std::make_shared<VulkanShader>(m_Context.GetDevice(),
                                                 fullPath.string());
    m_ShaderCache[name] = shader;
    return shader;
  } catch (const std::exception &e) {
    MC_ERROR("Failed to load shader {}: {}", name, e.what());
    return nullptr;
  }
}

void AssetManager::AddMesh(const std::string &name,
                           std::shared_ptr<VulkanMesh> mesh) {
  if (m_MeshCache.contains(name)) {
    MC_WARN("Overwriting existing mesh in cache: {}", name);
  }
  m_MeshCache[name] = mesh;
}

std::shared_ptr<VulkanMesh> AssetManager::GetMesh(const std::string &name) {
  if (m_MeshCache.contains(name)) {
    return m_MeshCache[name];
  }
  MC_WARN("Mesh not found in cache: {}", name);
  return nullptr;
}

std::filesystem::path
AssetManager::GetFullPath(const std::string &relativePath) const {
  return m_BaseDir / relativePath;
}

} // namespace mc
