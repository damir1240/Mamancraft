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

  // Prepare window flags. For Vulkan, we need SDL_WINDOW_VULKAN.
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

  const char *basePath = SDL_GetBasePath();
  std::string shadersPath = std::string(basePath ? basePath : "") + "shaders/";

  VulkanShader vertShader(m_VulkanContext->GetDevice(),
                          shadersPath + "triangle.vert.spv");
  VulkanShader fragShader(m_VulkanContext->GetDevice(),
                          shadersPath + "triangle.frag.spv");

  PipelineConfigInfo pipelineConfig;
  VulkanPipeline::DefaultPipelineConfigInfo(pipelineConfig);
  pipelineConfig.colorAttachmentFormat =
      m_VulkanContext->GetSwapchain()->GetImageFormat();

  m_Pipeline = std::make_unique<VulkanPipeline>(
      m_VulkanContext->GetDevice(), vertShader, fragShader, pipelineConfig);
  m_Renderer = std::make_unique<VulkanRenderer>(*m_VulkanContext);

  const std::vector<Vertex> vertices = {{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                                        {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
                                        {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}};

  vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

  VulkanBuffer stagingBuffer(
      m_VulkanContext->GetAllocator()->GetAllocator(), bufferSize, 1,
      vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_CPU_ONLY);
  stagingBuffer.Map();
  stagingBuffer.WriteToBuffer((void *)vertices.data(), bufferSize);
  stagingBuffer.Unmap();

  m_VertexBuffer = std::make_unique<VulkanBuffer>(
      m_VulkanContext->GetAllocator()->GetAllocator(), bufferSize, 1,
      vk::BufferUsageFlagBits::eVertexBuffer |
          vk::BufferUsageFlagBits::eTransferDst,
      VMA_MEMORY_USAGE_GPU_ONLY);

  VulkanBuffer::CopyBuffer(
      m_VulkanContext->GetDevice(), m_VulkanContext->GetCommandPool(),
      stagingBuffer.GetBuffer(), m_VertexBuffer->GetBuffer(), bufferSize);

  m_IsRunning = true;
  MC_INFO("Application initialized successfully.");
}

void Application::Shutdown() {
  MC_INFO("Shutting down Application.");

  m_Renderer.reset();
  m_Pipeline.reset();
  m_VertexBuffer.reset();
  m_VulkanContext.reset();

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

    // Handle window close event from the 'X' button
    if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
        event.window.windowID == SDL_GetWindowID(m_Window)) {
      MC_INFO("Window close request received.");
      m_IsRunning = false;
    }

    // Handle escape key to quit
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
      m_Renderer->Draw(commandBuffer, *m_Pipeline, m_VertexBuffer->GetBuffer());
      m_Renderer->EndRenderPass(commandBuffer);
      m_Renderer->EndFrame();
    }
  }
}

} // namespace mc
