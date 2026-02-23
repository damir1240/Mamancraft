#include "Mamancraft/Core/Application.hpp"
#include "Mamancraft/Core/FileSystem.hpp"
#include "Mamancraft/Core/Logger.hpp"
#include "Mamancraft/Renderer/VulkanContext.hpp"
#include "Mamancraft/Voxel/BlockRegistry.hpp"
#include "Mamancraft/Voxel/Chunk.hpp"
#include "Mamancraft/Voxel/TerrainGenerator.hpp"

#include <SDL3/SDL_assert.h>
#include <chrono>
#include <spdlog/fmt/fmt.h>
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
  m_InputManager->BindAction("Descend", SDL_SCANCODE_LSHIFT);
  m_InputManager->BindAction("MoveForward", SDL_SCANCODE_W);
  m_InputManager->BindAction("MoveBackward", SDL_SCANCODE_S);
  m_InputManager->BindAction("MoveLeft", SDL_SCANCODE_A);
  m_InputManager->BindAction("MoveRight", SDL_SCANCODE_D);
  m_InputManager->BindAction("Menu", SDL_SCANCODE_ESCAPE);
  m_InputManager->BindAction("ToggleCursor", SDL_SCANCODE_M);
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

  // --- Initialize Block Registry Textures ---
  MC_INFO("Loading block textures...");
  auto &registry = BlockRegistry::Instance();
  std::unordered_map<std::string, uint32_t> textureToIndex;

  for (auto &[type, info] : registry.GetMutableRegistry()) {
    if (type == BlockType::Air)
      continue;

    auto registerTex = [&](const std::string &path, uint32_t &outIndex) {
      if (path.empty())
        return;
      if (textureToIndex.contains(path)) {
        outIndex = textureToIndex[path];
        return;
      }
      auto handle = m_AssetManager->LoadTexture(path);
      auto tex = m_AssetManager->GetTexture(handle);
      if (tex) {
        outIndex = m_Renderer->RegisterTexture(*tex);
        textureToIndex[path] = outIndex;
      } else {
        MC_ERROR("Failed to load block texture: {0}", path);
      }
    };

    registerTex(info.textureTop, info.texIndexTop);
    registerTex(info.textureSide, info.texIndexSide);
    registerTex(info.textureBottom, info.texIndexBottom);
  }

  PipelineConfigInfo pipelineConfig;
  VulkanPipeline::DefaultPipelineConfigInfo(pipelineConfig);
  pipelineConfig.colorAttachmentFormat =
      m_VulkanContext->GetSwapchain()->GetImageFormat();
  pipelineConfig.depthAttachmentFormat =
      m_VulkanContext->GetSwapchain()->GetDepthFormat();

  pipelineConfig.descriptorSetLayouts = {
      m_Renderer->GetGlobalDescriptorSetLayout(),
      m_Renderer->GetBindlessDescriptorSetLayout()};

  vk::PushConstantRange pushConstantRange{};
  pushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex;
  pushConstantRange.offset = 0;
  pushConstantRange.size = sizeof(PushConstantData);
  pipelineConfig.pushConstantRanges = {pushConstantRange};

  m_Pipeline = std::make_unique<VulkanPipeline>(
      m_VulkanContext->GetDevice(), *vertShader, *fragShader, pipelineConfig);

  m_TaskSystem = std::make_unique<TaskSystem>();
  m_World = std::make_unique<World>(std::make_unique<WaveTerrainGenerator>(),
                                    *m_TaskSystem);

  // --- Initial World Load ---
  // Position camera first so World::Update knows what to load
  m_Camera.SetPosition(
      {Chunk::SIZE / 2.0f, Chunk::SIZE / 2.0f + 10.0f, Chunk::SIZE * 1.5f});
  m_Camera.SetPerspective(glm::radians(45.0f),
                          (float)m_Config.width / (float)m_Config.height, 0.1f,
                          1000.0f);

  m_World->Update(m_Camera.GetPosition());

  m_IsRunning = true;
  MC_INFO("Application initialized successfully with TaskSystem: {0} threads",
          m_TaskSystem->GetThreadCount());
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
  if (m_World)
    m_World.reset();
  if (m_TaskSystem)
    m_TaskSystem.reset();
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

  if (m_InputManager->IsActionPressed("ToggleCursor")) {
    m_InputManager->SetCursorLocking(m_Window,
                                     !m_InputManager->IsCursorLocked());
  }

  float moveSpeed = 5.0f * dt;
  glm::vec3 pos = m_Camera.GetPosition();
  glm::vec3 rot = m_Camera.GetRotation();

  // Compute direction vectors for movement (on XZ plane)
  float yawRad = glm::radians(rot.y);
  glm::vec3 flatForward = {sin(yawRad), 0.0f, -cos(yawRad)};
  glm::vec3 flatRight = {cos(yawRad), 0.0f, sin(yawRad)};

  if (m_InputManager->IsActionHeld("MoveForward"))
    pos += flatForward * moveSpeed;
  if (m_InputManager->IsActionHeld("MoveBackward"))
    pos -= flatForward * moveSpeed;
  if (m_InputManager->IsActionHeld("MoveLeft"))
    pos -= flatRight * moveSpeed;
  if (m_InputManager->IsActionHeld("MoveRight"))
    pos += flatRight * moveSpeed;
  if (m_InputManager->IsActionHeld("Jump"))
    pos.y += moveSpeed;
  if (m_InputManager->IsActionHeld("Descend"))
    pos.y -= moveSpeed;

  if (m_InputManager->IsCursorLocked()) {
    glm::vec2 delta = m_InputManager->GetMouseDelta();
    float sensitivity = 0.1f;

    // Mouse RIGHT (delta.x > 0) should turn camera RIGHT (increase yaw)
    rot.y += delta.x * sensitivity;

    // Mouse DOWN (delta.y > 0) should tip camera DOWN (decrease pitch)
    rot.x -= delta.y * sensitivity;

    // Clamp pitch to prevent flipping
    rot.x = glm::clamp(rot.x, -89.0f, 89.0f);
  }

  m_Camera.SetPosition(pos);
  m_Camera.SetRotation(rot);
  m_Camera.Update();

  // World Update
  m_World->Update(pos);

  // Process newly generated meshes (GPU Upload)
  auto pendingMeshes = m_World->GetPendingMeshes();
  for (auto &[coords, builder] : pendingMeshes) {
    std::string meshName =
        fmt::format("chunk_{}_{}_{}", coords.x, coords.y, coords.z);

    // Create or Update mesh
    if (m_ChunkMeshes.contains(coords)) {
      // Skip for now
    } else {
      if (builder.vertices.empty()) {
        m_ChunkMeshes[coords] = 0; // Loaded but nothing to render
      } else {
        m_ChunkMeshes[coords] = m_AssetManager->CreateMesh(meshName, builder);
      }
    }
  }
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

      PushConstantData push{};
      push.model = glm::mat4(1.0f);

      for (auto &[coords, handle] : m_ChunkMeshes) {
        if (auto mesh = m_AssetManager->GetMesh(handle)) {
          m_Renderer->DrawMesh(commandBuffer, *m_Pipeline, *mesh, push);
        }
      }

      m_Renderer->EndRenderPass(commandBuffer);
      m_Renderer->EndFrame();
    }
  }
}

} // namespace mc
