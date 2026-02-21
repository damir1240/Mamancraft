#pragma once

#include <SDL3/SDL.h>
#include <cstdint>
#include <memory>
#include <string>

#include "Mamancraft/Renderer/Vertex.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanBuffer.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanPipeline.hpp"
#include "Mamancraft/Renderer/VulkanContext.hpp"
#include "Mamancraft/Renderer/VulkanRenderer.hpp"
#include <vector>

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
  std::unique_ptr<VulkanPipeline> m_Pipeline;
  std::unique_ptr<VulkanRenderer> m_Renderer;
  std::unique_ptr<VulkanBuffer> m_VertexBuffer;
  std::unique_ptr<VulkanBuffer> m_IndexBuffer;
  uint32_t m_IndexCount = 0;
};

} // namespace mc
