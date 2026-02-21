#include "Mamancraft/Core/Application.hpp"
#include "Mamancraft/Core/Logger.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanShader.hpp"
#include "Mamancraft/Renderer/VulkanContext.hpp"

#include <stdexcept>

namespace mc {

Application::Application(const AppConfig &config) : m_Config(config) { Init(); }

Application::~Application() { Shutdown(); }

void Application::Init() {
  MC_INFO("Initializing Application: {0} ({1}x{2})", m_Config.title,
          m_Config.width, m_Config.height);

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    MC_CRITICAL("Failed to initialize SDL: {0}", SDL_GetError());
    throw std::runtime_error("SDL Initialization failed");
  }

  SDL_WindowFlags windowFlags =
      (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN |
                        SDL_WINDOW_HIGH_PIXEL_DENSITY);

  m_Window =
      SDL_CreateWindow(m_Config.title.c_str(), static_cast<int>(m_Config.width),
                       static_cast<int>(m_Config.height), windowFlags);

  if (!m_Window) {
    MC_CRITICAL("Failed to create SDL window: {0}", SDL_GetError());
    SDL_Quit();
    throw std::runtime_error("SDL Window creation failed");
  }

  m_VulkanContext = std::make_unique<VulkanContext>(m_Window);
  m_AssetManager = std::make_unique<AssetManager>(*m_VulkanContext);

  // Load resources via handles (Best Practice)
  auto vertHandle = m_AssetManager->LoadShader("shaders/triangle.vert.spv");
  auto fragHandle = m_AssetManager->LoadShader("shaders/triangle.frag.spv");

  auto vertShader = m_AssetManager->GetShader(vertHandle);
  auto fragShader = m_AssetManager->GetShader(fragHandle);

  PipelineConfigInfo pipelineConfig;
  VulkanPipeline::DefaultPipelineConfigInfo(pipelineConfig);
  pipelineConfig.colorAttachmentFormat =
      m_VulkanContext->GetSwapchain()->GetImageFormat();

  m_Pipeline = std::make_unique<VulkanPipeline>(
      m_VulkanContext->GetDevice(), *vertShader, *fragShader, pipelineConfig);
  m_Renderer = std::make_unique<VulkanRenderer>(*m_VulkanContext);

  VulkanMesh::Builder meshBuilder;
  meshBuilder.vertices = {{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                          {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
                          {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
                          {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}};
  meshBuilder.indices = {0, 1, 2, 2, 3, 0};

  m_TriangleMesh = m_AssetManager->CreateMesh("triangle", meshBuilder);

  m_IsRunning = true;
  MC_INFO("Application initialized successfully.");
}

void Application::Shutdown() {
  MC_INFO("Shutting down Application.");

  // 1. Wait for GPU idle (Crucial Best Practice)
  if (m_VulkanContext && m_VulkanContext->GetDevice()) {
    m_VulkanContext->GetDevice()->GetLogicalDevice().waitIdle();
  }

  // 2. Destroy objects in reverse order of creation
  m_Renderer.reset();
  m_Pipeline.reset();

  if (m_AssetManager) {
    m_AssetManager->Clear(); // Explicitly clear caches to free VMA allocations
    m_AssetManager.reset();
  }

  m_VulkanContext.reset(); // VMA allocator is safe to destroy now

  if (m_Window) {
    SDL_DestroyWindow(m_Window);
    m_Window = nullptr;
  }

  SDL_Quit();
}

void Application::ProcessEvents() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_EVENT_QUIT) {
      MC_INFO("Quit event received. Stopping application.");
      m_IsRunning = false;
    }

    if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
        event.window.windowID == SDL_GetWindowID(m_Window)) {
      MC_INFO("Window close request received.");
      m_IsRunning = false;
    }

    if (event.type == SDL_EVENT_KEY_DOWN) {
      if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
        MC_INFO("Escape key pressed. Stopping application.");
        m_IsRunning = false;
      }
    }
  }
}

void Application::Update() {
  // Core logic update here
}

void Application::Run() {
  MC_INFO("Starting main loop.");
  while (m_IsRunning) {
    ProcessEvents();
    Update();

    if (auto commandBuffer = m_Renderer->BeginFrame()) {
      m_Renderer->BeginRenderPass(commandBuffer);

      // Draw using handles
      if (auto mesh = m_AssetManager->GetMesh(m_TriangleMesh)) {
        m_Renderer->DrawMesh(commandBuffer, *m_Pipeline, *mesh);
      }

      m_Renderer->EndRenderPass(commandBuffer);
      m_Renderer->EndFrame();
    }
  }
}

} // namespace mc
