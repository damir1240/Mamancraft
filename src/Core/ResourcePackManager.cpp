#include "Mamancraft/Core/ResourcePackManager.hpp"
#include "Mamancraft/Core/Logger.hpp"
#include <fstream>
#include <sstream>

namespace mc {

ResourcePackManager::ResourcePackManager(const std::filesystem::path &rootDir)
    : m_RootDir(rootDir) {
  m_BasePackDir = m_RootDir / "base";
  RefreshPacks();
}

void ResourcePackManager::RefreshPacks() {
  MC_INFO("Refreshing Resource Packs...");
  m_ActivePacks.clear();

  // 1. Scan for packs in "resourcepacks" folder
  std::filesystem::path packsDir = m_RootDir / "resourcepacks";
  if (std::filesystem::exists(packsDir) &&
      std::filesystem::is_directory(packsDir)) {
    for (const auto &entry : std::filesystem::directory_iterator(packsDir)) {
      if (entry.is_directory()) {
        auto metadata = LoadMetadata(entry.path());
        if (metadata) {
          MC_INFO("Found Resource Pack: {0}", metadata->name);
          m_ActivePacks.push_back({entry.path(), *metadata});
        }
      }
    }
  }

  // 2. Add Base Pack last (lowest priority)
  auto baseMeta = LoadMetadata(m_BasePackDir);
  if (baseMeta) {
    m_ActivePacks.push_back({m_BasePackDir, *baseMeta});
  } else {
    // Even without metadata, add it as fallback
    m_ActivePacks.push_back({m_BasePackDir, {"Base", "Built-in resources", 1}});
  }

  MC_INFO("Total Active Packs: {0}", m_ActivePacks.size());
}

std::optional<std::filesystem::path>
ResourcePackManager::ResolvePath(std::string_view namespacedPath) const {
  auto [ns, path] = SplitNamespace(namespacedPath);

  // Search through packs in priority order
  for (const auto &[dir, meta] : m_ActivePacks) {
    // Modern Minecraft structure: assets/<namespace>/<path>
    std::filesystem::path fullPath = dir / "assets" / ns / path;
    if (std::filesystem::exists(fullPath)) {
      return fullPath;
    }
  }

  return std::nullopt;
}

std::optional<ResourcePackManager::PackMetadata>
ResourcePackManager::LoadMetadata(const std::filesystem::path &packDir) {
  std::filesystem::path metaPath = packDir / "pack.json";
  if (!std::filesystem::exists(metaPath)) {
    return std::nullopt;
  }

  // Simple manual parsing for now (Avoids adding heavy JSON lib for just 3
  // strings) Best practice: Use nlohmann/json, but this keeps dependencies
  // light until needed
  PackMetadata meta{"Unknown", "No description", 1};

  std::ifstream f(metaPath);
  if (f.is_open()) {
    std::string line;
    while (std::getline(f, line)) {
      if (line.find("\"name\"") != std::string::npos) {
        size_t first = line.find_first_of('\"', line.find(':'));
        size_t last = line.find_last_of('\"');
        if (first != std::string::npos && last != std::string::npos)
          meta.name = line.substr(first + 1, last - first - 1);
      }
      if (line.find("\"description\"") != std::string::npos) {
        size_t first = line.find_first_of('\"', line.find(':'));
        size_t last = line.find_last_of('\"');
        if (first != std::string::npos && last != std::string::npos)
          meta.description = line.substr(first + 1, last - first - 1);
      }
    }
  }

  return meta;
}

std::pair<std::string, std::string>
ResourcePackManager::SplitNamespace(std::string_view path) {
  size_t colon = path.find(':');
  if (colon == std::string::npos) {
    return {"mc", std::string(path)}; // Default namespace
  }
  return {std::string(path.substr(0, colon)),
          std::string(path.substr(colon + 1))};
}

} // namespace mc
