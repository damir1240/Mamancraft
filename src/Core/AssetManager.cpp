#include "Mamancraft/Core/AssetManager.hpp"
#include "Mamancraft/Core/FileSystem.hpp"
#include "Mamancraft/Core/Logger.hpp"
#include "Mamancraft/Renderer/VulkanContext.hpp"

#include <SDL3/SDL.h>

namespace mc {

AssetManager::AssetManager(VulkanContext &context) : m_Context(context) {
  m_PackManager =
      std::make_unique<ResourcePackManager>(FileSystem::GetAssetsDir());
  MC_INFO("Modern AssetManager initialized with ResourcePackManager");
}

AssetManager::~AssetManager() {
  MC_DEBUG("AssetManager destructor: Checking if cleanup needed");
  Clear();
}

void AssetManager::Clear() {
  MC_INFO("AssetManager::Clear() - Starting cleanup");

  // Order matters: textures and meshes use Vulkan resources
  m_MeshCache.clear();
  m_TextureCache.clear();
  m_ShaderCache.clear();
  m_NameToHandle.clear();

  MC_INFO("AssetManager::Clear() - Cleanup completed");
}

AssetHandle AssetManager::GenerateHandle(std::string_view name) {
  return std::hash<std::string_view>{}(name);
}

AssetHandle AssetManager::LoadShader(std::string_view namespacedPath) {
  std::string sPath(namespacedPath);
  if (m_NameToHandle.contains(sPath)) {
    return m_NameToHandle[sPath];
  }

  auto resolved = m_PackManager->ResolvePath(namespacedPath);
  if (!resolved) {
    MC_ERROR("Failed to resolve shader path: {0}", namespacedPath);
    return INVALID_HANDLE;
  }

  AssetHandle handle = GenerateHandle(namespacedPath);
  try {
    auto shader = std::make_shared<VulkanShader>(m_Context.GetDevice(),
                                                 resolved->string());
    m_ShaderCache[handle] = shader;
    m_NameToHandle[sPath] = handle;
    return handle;
  } catch (const std::exception &e) {
    MC_ERROR("Failed to load shader {0}: {1}", namespacedPath, e.what());
    return INVALID_HANDLE;
  }
}

std::shared_ptr<VulkanShader> AssetManager::GetShader(AssetHandle handle) {
  auto it = m_ShaderCache.find(handle);
  return (it != m_ShaderCache.end()) ? it->second : nullptr;
}

AssetHandle AssetManager::LoadTexture(std::string_view namespacedPath) {
  std::string sPath(namespacedPath);
  if (m_NameToHandle.contains(sPath)) {
    return m_NameToHandle[sPath];
  }

  auto resolved = m_PackManager->ResolvePath(namespacedPath);
  if (!resolved) {
    MC_ERROR("Failed to resolve texture path: {0}", namespacedPath);
    return INVALID_HANDLE;
  }

  AssetHandle handle = GenerateHandle(namespacedPath);
  try {
    auto texture = std::make_shared<VulkanTexture>(m_Context, *resolved);
    m_TextureCache[handle] = texture;
    m_NameToHandle[sPath] = handle;
    return handle;
  } catch (const std::exception &e) {
    MC_ERROR("Failed to load texture {0}: {1}", namespacedPath, e.what());
    return INVALID_HANDLE;
  }
}

std::shared_ptr<VulkanTexture> AssetManager::GetTexture(AssetHandle handle) {
  auto it = m_TextureCache.find(handle);
  return (it != m_TextureCache.end()) ? it->second : nullptr;
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
  return (it != m_MeshCache.end()) ? it->second : nullptr;
}

} // namespace mc
