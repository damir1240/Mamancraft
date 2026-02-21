#include "Mamancraft/Core/AssetManager.hpp"
#include "Mamancraft/Core/Logger.hpp"
#include "Mamancraft/Renderer/VulkanContext.hpp"


#include <SDL3/SDL.h>

namespace mc {

AssetManager::AssetManager(VulkanContext &context) : m_Context(context) {
  // In SDL3, SDL_GetBasePath returns a const char* that does NOT need to be
  // freed. However, some versions might still return char*. Using const char*
  // is safer.
  const char *basePath = SDL_GetBasePath();
  if (basePath) {
    m_BaseDir = std::filesystem::path(basePath);
    // Note: SDL3 documentation states that for SDL_GetBasePath, you should NOT
    // free the string. It returns a pointer to internal memory.
  } else {
    MC_WARN("SDL_GetBasePath failed, using current directory for assets.");
    m_BaseDir = std::filesystem::current_path();
  }

  MC_INFO("Modern AssetManager initialized. Base directory: {}",
          m_BaseDir.string());
}

AssetManager::~AssetManager() { Clear(); }

void AssetManager::Clear() {
  if (!m_ShaderCache.empty() || !m_MeshCache.empty()) {
    MC_INFO("Clearing AssetManager caches (Best Practice Cleanup)");
    // Clean order: meshes (which hold buffers) then shaders
    m_MeshCache.clear();
    m_ShaderCache.clear();
    m_NameToHandle.clear();
  }
}

AssetHandle AssetManager::GenerateHandle(std::string_view name) {
  return std::hash<std::string_view>{}(name);
}

AssetHandle AssetManager::LoadShader(std::string_view name) {
  std::string sName(name);
  if (m_NameToHandle.contains(sName)) {
    return m_NameToHandle[sName];
  }

  AssetHandle handle = GenerateHandle(name);
  std::filesystem::path fullPath = m_BaseDir / name;

  try {
    auto shader = std::make_shared<VulkanShader>(m_Context.GetDevice(),
                                                 fullPath.string());
    m_ShaderCache[handle] = shader;
    m_NameToHandle[sName] = handle;
    return handle;
  } catch (const std::exception &e) {
    MC_ERROR("Failed to load shader {}: {}", name, e.what());
    return INVALID_HANDLE;
  }
}

std::shared_ptr<VulkanShader> AssetManager::GetShader(AssetHandle handle) {
  auto it = m_ShaderCache.find(handle);
  if (it != m_ShaderCache.end()) {
    return it->second;
  }
  return nullptr;
}

AssetHandle AssetManager::CreateMesh(std::string_view name,
                                     const VulkanMesh::Builder &builder) {
  std::string sName(name);
  AssetHandle handle = GenerateHandle(name);

  auto mesh = std::make_shared<VulkanMesh>(m_Context, builder);
  m_MeshCache[handle] = mesh;
  m_NameToHandle[sName] = handle;

  return handle;
}

std::shared_ptr<VulkanMesh> AssetManager::GetMesh(AssetHandle handle) {
  auto it = m_MeshCache.find(handle);
  if (it != m_MeshCache.end()) {
    return it->second;
  }
  return nullptr;
}

} // namespace mc
