#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>


#include "Mamancraft/Renderer/Vulkan/VulkanMesh.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanShader.hpp"


namespace mc {

class VulkanContext;

/**
 * @brief Centralized system for loading and managing game assets.
 *
 * Follows Best Practices:
 * - Resource caching to prevent duplicate loads.
 * - Reference counting via std::shared_ptr.
 * - Centralized path management.
 */
class AssetManager {
public:
  AssetManager(VulkanContext &context);
  ~AssetManager();

  // Prevent copying
  AssetManager(const AssetManager &) = delete;
  AssetManager &operator=(const AssetManager &) = delete;

  /**
   * @brief Loads or retrieves a shader from the cache.
   * @param name The relative path to the shader (e.g.,
   * "shaders/triangle.vert.spv")
   */
  std::shared_ptr<VulkanShader> GetShader(const std::string &name);

  /**
   * @brief Manually adds a mesh to the manager (useful for procedurally
   * generated meshes like Chunks).
   * @param name Unique name for the mesh.
   * @param mesh shared_ptr to the mesh.
   */
  void AddMesh(const std::string &name, std::shared_ptr<VulkanMesh> mesh);

  /**
   * @brief Retrieves a mesh by name.
   */
  std::shared_ptr<VulkanMesh> GetMesh(const std::string &name);

  /**
   * @brief Sets the base directory for assets.
   */
  void SetBaseDirectory(const std::filesystem::path &path) { m_BaseDir = path; }

private:
  std::filesystem::path GetFullPath(const std::string &relativePath) const;

private:
  VulkanContext &m_Context;
  std::filesystem::path m_BaseDir;

  std::unordered_map<std::string, std::shared_ptr<VulkanShader>> m_ShaderCache;
  std::unordered_map<std::string, std::shared_ptr<VulkanMesh>> m_MeshCache;
};

} // namespace mc
