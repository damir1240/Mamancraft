#include "Mamancraft/Core/Logger.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanMesh.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanPipeline.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanShader.hpp"
#include "Mamancraft/Renderer/VulkanContext.hpp"
#include "Mamancraft/Renderer/VulkanRenderer.hpp"
#include <SDL3/SDL.h>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
class VulkanPipelineTest : public ::testing::Test {
protected:
  static void SetUpTestSuite() {
    try {
      mc::Logger::Init();
    } catch (...) {
      // Ignore if already initialized
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
      throw std::runtime_error("SDL Init failed");
    }
  }

  static void TearDownTestSuite() { SDL_Quit(); }

  void SetUp() override {
    window = SDL_CreateWindow("Test Window", 800, 600,
                              SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN);
    if (!window) {
      GTEST_SKIP() << "Skipping test: SDL_CreateWindow failed (likely no "
                      "Vulkan support on this machine). SDL Error: "
                   << SDL_GetError();
      return;
    }

    try {
      context = std::make_unique<mc::VulkanContext>(window);
    } catch (const std::exception &e) {
      GTEST_SKIP() << "Skipping test: VulkanContext initialization failed "
                      "(likely no compatible GPU found). Error: "
                   << e.what();
      return;
    }
  }

  void TearDown() override {
    context.reset();
    if (window) {
      SDL_DestroyWindow(window);
    }
  }

  SDL_Window *window = nullptr;
  std::unique_ptr<mc::VulkanContext> context;
};

TEST_F(VulkanPipelineTest, CreateShaderModule) {
  const char *basePath = SDL_GetBasePath();
  std::string shadersPath =
      std::string(basePath ? basePath : "") + "assets/base/assets/mc/shaders/";

  // Test loading standard shaders
  EXPECT_NO_THROW({
    mc::VulkanShader vertShader(context->GetDevice(),
                                shadersPath + "voxel.vert.spv");
    EXPECT_NE(vertShader.GetShaderModule(), VK_NULL_HANDLE);
  });
}

TEST_F(VulkanPipelineTest, CreateVulkanPipeline) {
  const char *basePath = SDL_GetBasePath();
  std::string shadersPath =
      std::string(basePath ? basePath : "") + "assets/base/assets/mc/shaders/";

  mc::VulkanShader vertShader(context->GetDevice(),
                              shadersPath + "voxel.vert.spv");
  mc::VulkanShader fragShader(context->GetDevice(),
                              shadersPath + "voxel.frag.spv");

  mc::VulkanRenderer renderer(*context);

  mc::PipelineConfigInfo configInfo;
  mc::VulkanPipeline::DefaultPipelineConfigInfo(configInfo);
  configInfo.colorAttachmentFormat = context->GetSwapchain()->GetImageFormat();
  configInfo.depthAttachmentFormat = context->GetSwapchain()->GetDepthFormat();

  configInfo.descriptorSetLayouts = {renderer.GetGlobalDescriptorSetLayout(),
                                     renderer.GetBindlessDescriptorSetLayout()};

  EXPECT_NO_THROW({
    mc::VulkanPipeline pipeline(context->GetDevice(), vertShader, fragShader,
                                configInfo);
    EXPECT_NE(pipeline.GetPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(pipeline.GetPipelineLayout(), VK_NULL_HANDLE);
  });
}

TEST_F(VulkanPipelineTest, CreateVulkanRenderer) {
  EXPECT_NO_THROW({ mc::VulkanRenderer renderer(*context); });
}

TEST_F(VulkanPipelineTest, CreateVulkanMesh) {
  mc::VulkanMesh::Builder builder;
  builder.vertices = {{{0.0f, -0.5f, 0.0f}, 0, {0.0f, 0.0f}},
                      {{0.5f, 0.5f, 0.0f}, 0, {1.0f, 0.0f}},
                      {{-0.5f, 0.5f, 0.0f}, 0, {0.0f, 1.0f}}};
  builder.indices = {0, 1, 2};

  EXPECT_NO_THROW({ mc::VulkanMesh mesh(*context, builder); });
}
