#include "Mamancraft/Core/Application.hpp"
#include "Mamancraft/Core/FileSystem.hpp"
#include "Mamancraft/Core/Logger.hpp"
#include "Mamancraft/Renderer/VulkanContext.hpp"
#include "Mamancraft/Voxel/BlockRegistry.hpp"
#include "Mamancraft/Voxel/Chunk.hpp"
#include "Mamancraft/Voxel/TerrainGenerator.hpp"

#include <SDL3/SDL_assert.h>
#include <chrono>
#include <random>
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
  m_InputManager->BindAction("Speed1", SDL_SCANCODE_1);
  m_InputManager->BindAction("Speed2", SDL_SCANCODE_2);
  m_InputManager->BindAction("Speed3", SDL_SCANCODE_3);
  m_InputManager->BindAction("Speed4", SDL_SCANCODE_4);
  m_InputManager->BindAction("Speed5", SDL_SCANCODE_5);
  m_InputManager->BindMouseButton("Interact", SDL_BUTTON_LEFT);

  // Load User Configuration (Overrides Defaults)
  std::string configPath = (FileSystem::GetConfigDir() / "input.cfg").string();
  m_InputManager->LoadConfiguration(configPath);
  m_InputManager->SaveConfiguration(configPath);

  // Load resources via handles (Best Practice)
  auto vertHandle = m_AssetManager->LoadShader("shaders/voxel.vert.spv");
  auto fragHandle = m_AssetManager->LoadShader("shaders/voxel.frag.spv");

  auto vertShader = m_AssetManager->GetShader(vertHandle);
  auto fragShader = m_AssetManager->GetShader(fragHandle);

  if (!vertShader || !fragShader) {
    MC_CRITICAL("Required shaders failed to load!");
    throw std::runtime_error("Required shaders failed to load");
  }

  // --- Initialize Block Registry Textures & Materials ---
  MC_INFO("Loading block textures and materials...");
  auto &registry = BlockRegistry::Instance();
  std::unordered_map<std::string, uint32_t> textureToIndex;

  m_MaterialSystem = std::make_unique<MaterialSystem>(*m_VulkanContext);

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

    // Register MaterialData for SSBO
    MaterialData matData;
    matData.albedoTexIndex = info.texIndexSide; // Default to side texture
    if (info.textureSide.empty()) {
      matData.albedoTexIndex = info.texIndexTop;
    }
    matData.animFrames = info.animFrames;
    matData.albedoTint = glm::vec4(info.color, 1.0f);
    matData.flags = info.isTransparent ? 1 : 0;

    // FPS could be added to info later, default to 8.0f
    matData.animFPS = 8.0f;

    info.materialID = m_MaterialSystem->RegisterMaterial(matData);
  }

  m_MaterialSystem->UploadToGPU();

  // Initialize GPU-Driven Systems (VRAM: ~128MB Vertex, ~64MB Index buffer)
  m_IndirectDrawSystem = std::make_unique<IndirectDrawSystem>(
      *m_VulkanContext, 10000, 128 * 1024 * 1024, 64 * 1024 * 1024);
  m_CullingSystem = std::make_unique<CullingSystem>(*m_VulkanContext);

  PipelineConfigInfo pipelineConfig;
  VulkanPipeline::DefaultPipelineConfigInfo(pipelineConfig);
  pipelineConfig.colorAttachmentFormat =
      m_VulkanContext->GetSwapchain()->GetImageFormat();
  pipelineConfig.depthAttachmentFormat =
      m_VulkanContext->GetSwapchain()->GetDepthFormat();

  pipelineConfig.descriptorSetLayouts = {
      m_Renderer->GetGlobalDescriptorSetLayout(),
      m_Renderer->GetBindlessDescriptorSetLayout()};

  m_Pipeline = std::make_unique<VulkanPipeline>(
      m_VulkanContext->GetDevice(), *vertShader, *fragShader, pipelineConfig);

  m_TaskSystem = std::make_unique<TaskSystem>();

  // Random world seed based on system time
  uint32_t worldSeed = static_cast<uint32_t>(
      std::chrono::steady_clock::now().time_since_epoch().count() & 0xFFFFFFFF);
  MC_INFO("World seed: {}", worldSeed);

  m_World = std::make_unique<World>(
      std::make_unique<AdvancedTerrainGenerator>(worldSeed), *m_TaskSystem);

  // --- Initial World Load ---
  // Position camera just above expected Minecraft-style terrain (base ~64)
  m_Camera.SetPosition({Chunk::SIZE / 2.0f, 80.0f, Chunk::SIZE / 2.0f});
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

  // CRITICAL: Signal world to abort in-flight generation tasks,
  // then stop TaskSystem. This makes shutdown near-instant.
  if (m_World)
    m_World->SignalShutdown();
  if (m_TaskSystem)
    m_TaskSystem.reset();

  // Now safe to destroy World (no more background tasks running)
  if (m_World)
    m_World.reset();

  if (m_CullingSystem)
    m_CullingSystem.reset();
  if (m_IndirectDrawSystem)
    m_IndirectDrawSystem.reset();
  if (m_MaterialSystem)
    m_MaterialSystem.reset();

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
          (float)event.window.data1 / (float)event.window.data2, 0.1f, 1000.0f);
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

  float moveSpeed = m_FlightSpeed * dt;
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

  // Flight speed control (keys 1-5)
  if (m_InputManager->IsActionPressed("Speed1"))
    m_FlightSpeed = 2.0f;
  if (m_InputManager->IsActionPressed("Speed2"))
    m_FlightSpeed = 5.0f;
  if (m_InputManager->IsActionPressed("Speed3"))
    m_FlightSpeed = 15.0f;
  if (m_InputManager->IsActionPressed("Speed4"))
    m_FlightSpeed = 50.0f;
  if (m_InputManager->IsActionPressed("Speed5"))
    m_FlightSpeed = 200.0f;

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
    if (m_ChunkDrawIDs.contains(coords)) {
      m_IndirectDrawSystem->RemoveChunk(m_ChunkDrawIDs[coords]);
      m_ChunkDrawIDs.erase(coords);
    }

    if (!builder.vertices.empty()) {
      ObjectData objData;
      // Vertices are already in world space, model matrix is identity
      objData.model = glm::mat4(1.0f);

      // AABB covers the chunk bounding box
      glm::vec3 worldPos = glm::vec3(coords) * static_cast<float>(Chunk::SIZE);
      objData.aabbMin = glm::vec4(worldPos, 1.0f);
      objData.aabbMax =
          glm::vec4(worldPos + static_cast<float>(Chunk::SIZE), 1.0f);

      uint32_t drawID = m_IndirectDrawSystem->AddChunk(
          builder.vertices, builder.indices, objData);
      m_ChunkDrawIDs[coords] = drawID;
    }
  }

  // Pre-frame flush of new CPU-side chunk allocations to GPU Mega Buffers
  m_IndirectDrawSystem->FlushToGPU();
}

void Application::Run() {
  MC_INFO("Starting main loop.");
  auto lastTime = std::chrono::high_resolution_clock::now();
  float totalTime = 0.0f; // accumulated elapsed time (for shader animation)

  while (m_IsRunning) {
    auto currentTime = std::chrono::high_resolution_clock::now();
    float dt = std::chrono::duration<float, std::chrono::seconds::period>(
                   currentTime - lastTime)
                   .count();
    lastTime = currentTime;
    totalTime += dt;

    m_InputManager->NewFrame();
    ProcessEvents();
    Update(dt);

    if (auto commandBuffer = m_Renderer->BeginFrame()) {
      GlobalUbo ubo{};
      ubo.projection = m_Camera.GetProjection();
      ubo.view = m_Camera.GetView();
      ubo.time = totalTime; // drives animated water frames
      m_Renderer->UpdateGlobalUbo(ubo);

      // Bind SSBOs to global descriptor sets
      m_Renderer->BindMaterialBuffer(*m_MaterialSystem);
      m_Renderer->BindObjectDataBuffer(
          m_IndirectDrawSystem->GetObjectDataBuffer(),
          m_IndirectDrawSystem->GetObjectDataBufferSize());

      // CPU: Mark all commands as visible (instanceCount = 1) and flush SSBOs
      m_IndirectDrawSystem->ResetDrawCommands();
      m_IndirectDrawSystem->FlushToGPU();

      // GPU Compute Frustum Culling
      CullUniforms cullUbo{};
      cullUbo.viewProj = ubo.projection * ubo.view;

      glm::mat4 vp = cullUbo.viewProj;
      cullUbo.frustumPlanes[0] =
          glm::vec4(vp[0][3] + vp[0][0], vp[1][3] + vp[1][0],
                    vp[2][3] + vp[2][0], vp[3][3] + vp[3][0]); // Left
      cullUbo.frustumPlanes[1] =
          glm::vec4(vp[0][3] - vp[0][0], vp[1][3] - vp[1][0],
                    vp[2][3] - vp[2][0], vp[3][3] - vp[3][0]); // Right
      cullUbo.frustumPlanes[2] =
          glm::vec4(vp[0][3] + vp[0][1], vp[1][3] + vp[1][1],
                    vp[2][3] + vp[2][1], vp[3][3] + vp[3][1]); // Bottom
      cullUbo.frustumPlanes[3] =
          glm::vec4(vp[0][3] - vp[0][1], vp[1][3] - vp[1][1],
                    vp[2][3] - vp[2][1], vp[3][3] - vp[3][1]); // Top
      cullUbo.frustumPlanes[4] =
          glm::vec4(vp[0][2], vp[1][2], vp[2][2], vp[3][2]); // Near
      cullUbo.frustumPlanes[5] =
          glm::vec4(vp[0][3] - vp[0][2], vp[1][3] - vp[1][2],
                    vp[2][3] - vp[2][2], vp[3][3] - vp[3][2]); // Far

      for (int i = 0; i < 6; i++) {
        cullUbo.frustumPlanes[i] /=
            glm::length(glm::vec3(cullUbo.frustumPlanes[i]));
      }

      cullUbo.drawCount = m_IndirectDrawSystem->GetActiveDrawCount();

      // Dispatch culling shader
      m_CullingSystem->Execute(commandBuffer, cullUbo,
                               m_IndirectDrawSystem->GetDrawCommandBuffer(),
                               m_IndirectDrawSystem->GetDrawCommandBufferSize(),
                               m_IndirectDrawSystem->GetObjectDataBuffer(),
                               m_IndirectDrawSystem->GetObjectDataBufferSize(),
                               m_IndirectDrawSystem->GetActiveDrawCount());

      // GPU Indirect Draw
      m_Renderer->BeginRenderPass(commandBuffer);
      m_Renderer->DrawIndirect(commandBuffer, *m_Pipeline,
                               *m_IndirectDrawSystem);
      m_Renderer->EndRenderPass(commandBuffer);
      m_Renderer->EndFrame();
    }
  }
}

} // namespace mc
