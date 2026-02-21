#include "Mamancraft/Core/AssetManager.hpp"
#include "Mamancraft/Core/FileSystem.hpp"
#include "Mamancraft/Core/Logger.hpp"
#include "Mamancraft/Renderer/VulkanContext.hpp"


#include <SDL3/SDL.h>

namespace mc {

AssetManager::AssetManager(VulkanContext &context) : m_Context(context) {
  m_BaseDir = FileSystem::GetAssetsDir();
  MC_INFO("Modern AssetManager initialized. Assets directory: {}",
          m_BaseDir.string());
}

AssetManager::~AssetManager() { Clear(); }

void AssetManager::Clear() {
  if (!m_ShaderCache.empty() || !m_MeshCache.empty()) {
    MC_INFO("AssetManager::Clear() - Starting cleanup");
    MC_DEBUG("AssetManager: Clearing {} meshes and {} shaders", 
             m_MeshCache.size(), m_ShaderCache.size());
    
    // CRITICAL: Clear meshes FIRST (they contain VulkanBuffers that use VMA)
    // This must happen BEFORE VulkanContext destroys the VMA allocator
    MC_DEBUG("AssetManager: Clearing mesh cache...");
    m_MeshCache.clear();
    
    MC_DEBUG("AssetManager: Clearing shader cache...");
    m_ShaderCache.clear();
    
    MC_DEBUG("AssetManager: Clearing name-to-handle map...");
    m_NameToHandle.clear();
    
    MC_INFO("AssetManager::Clear() - Cleanup completed");
  } else {
    MC_DEBUG("AssetManager::Clear() - No assets to clear");
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
