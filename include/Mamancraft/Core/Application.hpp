#pragma once

#include <SDL3/SDL.h>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include "Mamancraft/Core/AssetManager.hpp"
#include "Mamancraft/Core/InputManager.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanMesh.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanPipeline.hpp"
#include "Mamancraft/Renderer/VulkanContext.hpp"
#include "Mamancraft/Renderer/VulkanRenderer.hpp"

namespace mc {
struct AppConfig {
  std::string title = "Mamancraft";
  uint32_t width = 1280;
  uint32_t height = 720;
};

class Application {
public:
  Application(const AppConfig &config);
  ~Application();

  // Prevent copying
  Application(const Application &) = delete;
  Application &operator=(const Application &) = delete;

  void Run();

private:
  void Init();
  void Shutdown();
  void ProcessEvents();
  void Update();

private:
  AppConfig m_Config;
  bool m_IsRunning = false;
  SDL_Window *m_Window = nullptr;
  std::unique_ptr<VulkanContext> m_VulkanContext;
  std::unique_ptr<AssetManager> m_AssetManager;
  std::unique_ptr<InputManager> m_InputManager;
  std::unique_ptr<VulkanPipeline> m_Pipeline;
  std::unique_ptr<VulkanRenderer> m_Renderer;

  AssetHandle m_TriangleMesh = INVALID_HANDLE;
};

} // namespace mc
