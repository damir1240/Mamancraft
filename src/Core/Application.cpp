#include "Mamancraft/Core/Application.hpp"
#include "Mamancraft/Core/FileSystem.hpp"
#include "Mamancraft/Core/Logger.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanShader.hpp"
#include "Mamancraft/Renderer/VulkanContext.hpp"

#include <SDL3/SDL_assert.h>
#include <chrono>
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
    throw std::runtime_error("SDL Window creation failed");
  }

  m_VulkanContext = std::make_unique<VulkanContext>(m_Window);
  m_Renderer = std::make_unique<VulkanRenderer>(*m_VulkanContext);
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
  m_InputManager->SaveConfiguration(configPath);

  // Load resources via handles (Best Practice)
  auto vertHandle = m_AssetManager->LoadShader("shaders/triangle.vert.spv");
  auto fragHandle = m_AssetManager->LoadShader("shaders/triangle.frag.spv");

  auto vertShader = m_AssetManager->GetShader(vertHandle);
  auto fragShader = m_AssetManager->GetShader(fragHandle);

  if (!vertShader || !fragShader) {
    MC_CRITICAL("Required shaders failed to load!");
    throw std::runtime_error("Required shaders failed to load");
  }

  PipelineConfigInfo pipelineConfig;
  VulkanPipeline::DefaultPipelineConfigInfo(pipelineConfig);
  pipelineConfig.colorAttachmentFormat =
      m_VulkanContext->GetSwapchain()->GetImageFormat();
  pipelineConfig.depthAttachmentFormat =
      m_VulkanContext->GetSwapchain()->GetDepthFormat();

  pipelineConfig.descriptorSetLayouts = {
      m_Renderer->GetGlobalDescriptorSetLayout()};

  vk::PushConstantRange pushConstantRange{};
  pushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex;
  pushConstantRange.offset = 0;
  pushConstantRange.size = sizeof(PushConstantData);
  pipelineConfig.pushConstantRanges = {pushConstantRange};

  m_Pipeline = std::make_unique<VulkanPipeline>(
      m_VulkanContext->GetDevice(), *vertShader, *fragShader, pipelineConfig);

  VulkanMesh::Builder meshBuilder;
  // 3D Cube-like triangle or quad
  meshBuilder.vertices = {{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
                          {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
                          {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
                          {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}}};
  meshBuilder.indices = {0, 1, 2, 2, 3, 0};

  m_TriangleMesh = m_AssetManager->CreateMesh("triangle", meshBuilder);

  m_Camera.SetPosition({0.0f, 0.0f, 2.0f});
  m_Camera.SetPerspective(glm::radians(45.0f),
                          (float)m_Config.width / (float)m_Config.height, 0.1f,
                          100.0f);

  m_IsRunning = true;
  MC_INFO("Application initialized successfully.");
}

void Application::Shutdown() {
  MC_INFO("Application::Shutdown() - Starting shutdown sequence");

  if (m_Renderer)
    m_Renderer.reset();
  if (m_Pipeline)
    m_Pipeline.reset();
  if (m_AssetManager) {
    m_AssetManager->Clear();
    m_AssetManager.reset();
  }
  if (m_VulkanContext)
    m_VulkanContext.reset();
  if (m_InputManager)
    m_InputManager.reset();
  if (m_Window) {
    SDL_DestroyWindow(m_Window);
    m_Window = nullptr;
  }
}

void Application::ProcessEvents() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    m_InputManager->HandleEvent(event);
    if (event.type == SDL_EVENT_QUIT)
      m_IsRunning = false;
    if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
        event.window.windowID == SDL_GetWindowID(m_Window)) {
      m_IsRunning = false;
    }
    if (event.type == SDL_EVENT_WINDOW_RESIZED) {
      m_Camera.SetPerspective(
          glm::radians(45.0f),
          (float)event.window.data1 / (float)event.window.data2, 0.1f, 100.0f);
    }
  }
}

void Application::Update(float dt) {
  if (m_InputManager->IsActionPressed("Menu"))
    m_IsRunning = false;

  if (m_InputManager->IsKeyPressed(SDL_SCANCODE_M)) {
    m_InputManager->SetCursorLocking(m_Window,
                                     !m_InputManager->IsCursorLocked());
  }

  float moveSpeed = 5.0f * dt;
  glm::vec3 pos = m_Camera.GetPosition();
  glm::vec3 rot = m_Camera.GetRotation();

  if (m_InputManager->IsActionHeld("MoveForward"))
    pos.z -= moveSpeed;
  if (m_InputManager->IsActionHeld("MoveBackward"))
    pos.z += moveSpeed;
  if (m_InputManager->IsActionHeld("MoveLeft"))
    pos.x -= moveSpeed;
  if (m_InputManager->IsActionHeld("MoveRight"))
    pos.x += moveSpeed;
  if (m_InputManager->IsActionHeld("Jump"))
    pos.y += moveSpeed;

  if (m_InputManager->IsCursorLocked()) {
    glm::vec2 delta = m_InputManager->GetMouseDelta();
    rot.y -= delta.x * 0.1f;
    rot.x -= delta.y * 0.1f;
    rot.x = glm::clamp(rot.x, -89.0f, 89.0f);
  }

  m_Camera.SetPosition(pos);
  m_Camera.SetRotation(rot);
  m_Camera.Update();
}

void Application::Run() {
  MC_INFO("Starting main loop.");
  auto lastTime = std::chrono::high_resolution_clock::now();

  while (m_IsRunning) {
    auto currentTime = std::chrono::high_resolution_clock::now();
    float dt = std::chrono::duration<float, std::chrono::seconds::period>(
                   currentTime - lastTime)
                   .count();
    lastTime = currentTime;

    m_InputManager->NewFrame();
    ProcessEvents();
    Update(dt);

    if (auto commandBuffer = m_Renderer->BeginFrame()) {
      GlobalUbo ubo{};
      ubo.projection = m_Camera.GetProjection();
      ubo.view = m_Camera.GetView();
      m_Renderer->UpdateGlobalUbo(ubo);

      m_Renderer->BeginRenderPass(commandBuffer);

      if (auto mesh = m_AssetManager->GetMesh(m_TriangleMesh)) {
        PushConstantData push{};
        push.model = glm::mat4(1.0f); // Static triangle for now
        m_Renderer->DrawMesh(commandBuffer, *m_Pipeline, *mesh, push);
      }

      m_Renderer->EndRenderPass(commandBuffer);
      m_Renderer->EndFrame();
    }
  }
}

} // namespace mc
