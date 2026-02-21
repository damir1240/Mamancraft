#include "Mamancraft/Core/Application.hpp"
#include "Mamancraft/Core/FileSystem.hpp"
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
  m_InputManager = std::make_unique<InputManager>();

  // Set Default Key Bindings
  m_InputManager->BindAction("Jump", SDL_SCANCODE_SPACE);
  m_InputManager->BindAction("MoveForward", SDL_SCANCODE_W);
  m_InputManager->BindAction("MoveBackward", SDL_SCANCODE_S);
  m_InputManager->BindAction("MoveLeft", SDL_SCANCODE_A);
  m_InputManager->BindAction("MoveRight", SDL_SCANCODE_D);
  m_InputManager->BindAction("Menu", SDL_SCANCODE_ESCAPE);
  m_InputManager->BindMouseButton("Interact", SDL_BUTTON_LEFT);

  // Load User Configuration (Overrides Defaults)
  std::string configPath = (FileSystem::GetConfigDir() / "input.cfg").string();
  m_InputManager->LoadConfiguration(configPath);

  // Save current configuration back to ensure the file exists and is up to date
  m_InputManager->SaveConfiguration(configPath);

  // Load resources via handles (Best Practice)
  auto vertHandle = m_AssetManager->LoadShader("shaders/triangle.vert.spv");
  auto fragHandle = m_AssetManager->LoadShader("shaders/triangle.frag.spv");

  auto vertShader = m_AssetManager->GetShader(vertHandle);
  auto fragShader = m_AssetManager->GetShader(fragHandle);

  if (!vertShader || !fragShader) {
    MC_CRITICAL(
        "Required shaders failed to load! Check your assets directory.");
    throw std::runtime_error("Required shaders failed to load");
  }

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

  // Destroy objects in reverse order of creation
  // VulkanContext::Shutdown() will call waitIdle() internally
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

  m_InputManager.reset();
  SDL_Quit();
}

void Application::ProcessEvents() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    m_InputManager->HandleEvent(event);

    if (event.type == SDL_EVENT_QUIT) {
      MC_INFO("Quit event received. Stopping application.");
      m_IsRunning = false;
    }

    if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
        event.window.windowID == SDL_GetWindowID(m_Window)) {
      MC_INFO("Window close request received.");
      m_IsRunning = false;
    }
  }
}

void Application::Update() {
  if (m_InputManager->IsActionPressed("Menu")) {
    MC_INFO("Menu Action triggered. Stopping application.");
    m_IsRunning = false;
  }

  // Toggle cursor locking with 'M' key
  if (m_InputManager->IsKeyPressed(SDL_SCANCODE_M)) {
    m_InputManager->SetCursorLocking(m_Window,
                                     !m_InputManager->IsCursorLocked());
  }

  if (m_InputManager->IsCursorLocked()) {
    glm::vec2 delta = m_InputManager->GetMouseDelta();
    if (glm::length(delta) > 0.0f) {
      // Future: Update camera rotation here
    }
  }
}

void Application::Run() {
  MC_INFO("Starting main loop.");
  while (m_IsRunning) {
    m_InputManager->NewFrame();
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
