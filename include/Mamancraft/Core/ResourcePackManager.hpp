#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>


namespace mc {

/**
 * @brief Manage resource packs and asset overriding logic.
 * Follows a Minecraft-like stacking system.
 */
class ResourcePackManager {
public:
  struct PackMetadata {
    std::string name;
    std::string description;
    int formatVersion;
  };

  ResourcePackManager(const std::filesystem::path &rootDir);
  ~ResourcePackManager() = default;

  /**
   * @brief Refreshes the list of available packs and updates the active stack.
   */
  void RefreshPacks();

  /**
   * @brief Resolves a namespaced asset path (e.g.,
   * "mc:textures/block/dirt.png") to an absolute filesystem path, taking pack
   * overrides into account.
   */
  [[nodiscard]] std::optional<std::filesystem::path>
  ResolvePath(std::string_view namespacedPath) const;

  /**
   * @brief Gets metadata for all active packs in priority order (0 = highest).
   */
  [[nodiscard]] const std::vector<
      std::pair<std::filesystem::path, PackMetadata>> &
  GetActivePacks() const {
    return m_ActivePacks;
  }

private:
  void LoadBasePack();
  std::optional<PackMetadata>
  LoadMetadata(const std::filesystem::path &packDir);

  // Splits "namespace:path" into {"namespace", "path"}
  static std::pair<std::string, std::string>
  SplitNamespace(std::string_view path);

private:
  std::filesystem::path m_RootDir;

  // Priority list: First element has highest priority
  std::vector<std::pair<std::filesystem::path, PackMetadata>> m_ActivePacks;

  // The base/fallback pack directory
  std::filesystem::path m_BasePackDir;
};

} // namespace mc
