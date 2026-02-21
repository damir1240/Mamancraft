#pragma once

#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>


#include "Mamancraft/Renderer/Vulkan/VulkanMesh.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanShader.hpp"


namespace mc {

class VulkanContext;

/**
 * @brief Handle-based Asset Manager for modern Vulkan engines.
 *
 * Best Practices:
 * - Opaque handles (uint64_t) instead of smart pointers.
 * - Resource lifetime is managed internally.
 * - Centralized registry.
 * - C++20 features (std::span, std::string_view).
 */

using AssetHandle = uint64_t;
constexpr AssetHandle INVALID_HANDLE = 0;

class AssetManager {
public:
  AssetManager(VulkanContext &context);
  ~AssetManager();

  AssetManager(const AssetManager &) = delete;
  AssetManager &operator=(const AssetManager &) = delete;

  // --- Shaders ---
  AssetHandle LoadShader(std::string_view name);
  std::shared_ptr<VulkanShader> GetShader(AssetHandle handle);

  // --- Meshes ---
  AssetHandle CreateMesh(std::string_view name,
                         const VulkanMesh::Builder &builder);
  std::shared_ptr<VulkanMesh> GetMesh(AssetHandle handle);

  /**
   * @brief Forces destruction of all cached resources.
   * Must be called BEFORE destroying VulkanContext.
   */
  void Clear();

private:
  AssetHandle GenerateHandle(std::string_view name);

private:
  VulkanContext &m_Context;
  std::filesystem::path m_BaseDir;

  std::unordered_map<AssetHandle, std::shared_ptr<VulkanShader>> m_ShaderCache;
  std::unordered_map<AssetHandle, std::shared_ptr<VulkanMesh>> m_MeshCache;
  std::unordered_map<std::string, AssetHandle> m_NameToHandle;
};

} // namespace mc
