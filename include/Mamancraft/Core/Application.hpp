#pragma once

#include "Mamancraft/Core/AssetManager.hpp"
#include "Mamancraft/Core/InputManager.hpp"
#include "Mamancraft/Core/TaskSystem.hpp"

#include "Mamancraft/Renderer/Camera.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanMesh.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanPipeline.hpp"
#include "Mamancraft/Renderer/VulkanContext.hpp"
#include "Mamancraft/Renderer/VulkanRenderer.hpp"
#include "Mamancraft/Voxel/World.hpp"

#include <SDL3/SDL.h>
#include <memory>
#include <string>

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
  void Update(float dt);

private:
  AppConfig m_Config;
  SDL_Window *m_Window = nullptr;

  std::unique_ptr<VulkanContext> m_VulkanContext;
  std::unique_ptr<VulkanRenderer> m_Renderer;
  std::unique_ptr<AssetManager> m_AssetManager;
  std::unique_ptr<InputManager> m_InputManager;

  // Rendering
  std::unique_ptr<VulkanPipeline> m_Pipeline;
  std::unique_ptr<World> m_World;
  std::unordered_map<glm::ivec3, AssetHandle> m_ChunkMeshes;

  // Core (Task system must be destroyed before objects it references in tasks)
  std::unique_ptr<TaskSystem> m_TaskSystem;

  Camera m_Camera;

  bool m_IsRunning = false;
};

} // namespace mc
