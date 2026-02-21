#pragma once

#include <SDL3/SDL.h>
#include <filesystem>
#include <string>

namespace mc {

class FileSystem {
public:
  /**
   * @brief Gets the path to the directory where the executable is located.
   */
  static std::filesystem::path GetExecutableDir() {
    static std::filesystem::path execDir = []() {
      char *basePath = (char *)SDL_GetBasePath();
      if (basePath) {
        std::filesystem::path p(basePath);
        SDL_free(basePath);
        return p;
      }
      return std::filesystem::current_path();
    }();
    return execDir;
  }

  static std::filesystem::path GetConfigDir() {
    return GetExecutableDir() / "config";
  }

  static std::filesystem::path GetLogsDir() {
    return GetExecutableDir() / "logs";
  }

  static std::filesystem::path GetAssetsDir() {
    return GetExecutableDir() / "assets";
  }

  static void CreateDirectories() {
    std::filesystem::create_directories(GetConfigDir());
    std::filesystem::create_directories(GetLogsDir());
    std::filesystem::create_directories(GetAssetsDir());
  }
};

} // namespace mc
