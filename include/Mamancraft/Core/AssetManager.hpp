#pragma once

#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>

#include "Mamancraft/Core/ResourcePackManager.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanMesh.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanShader.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanTexture.hpp"

namespace mc {

class VulkanContext;

using AssetHandle = uint64_t;
constexpr AssetHandle INVALID_HANDLE = 0;

class AssetManager {
public:
  AssetManager(VulkanContext &context);
  ~AssetManager();

  AssetManager(const AssetManager &) = delete;
  AssetManager &operator=(const AssetManager &) = delete;

  // --- Resource Pack Management ---
  ResourcePackManager &GetPackManager() { return *m_PackManager; }

  // --- Shaders ---
  AssetHandle LoadShader(std::string_view namespacedPath);
  std::shared_ptr<VulkanShader> GetShader(AssetHandle handle);

  // --- Textures ---
  AssetHandle LoadTexture(std::string_view namespacedPath);
  std::shared_ptr<VulkanTexture> GetTexture(AssetHandle handle);

  // --- Meshes ---
  AssetHandle CreateMesh(std::string_view name,
                         const VulkanMesh::Builder &builder);
  std::shared_ptr<VulkanMesh> GetMesh(AssetHandle handle);

  void Clear();

private:
  AssetHandle GenerateHandle(std::string_view name);

private:
  VulkanContext &m_Context;
  std::unique_ptr<ResourcePackManager> m_PackManager;

  std::unordered_map<AssetHandle, std::shared_ptr<VulkanShader>> m_ShaderCache;
  std::unordered_map<AssetHandle, std::shared_ptr<VulkanTexture>>
      m_TextureCache;
  std::unordered_map<AssetHandle, std::shared_ptr<VulkanMesh>> m_MeshCache;
  std::unordered_map<std::string, AssetHandle> m_NameToHandle;
};

} // namespace mc
