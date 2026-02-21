#include "Mamancraft/Core/Logger.hpp"
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
    ASSERT_NE(window, nullptr) << "SDL Window could not be created!";

    // We expect some basic dummy shaders to be compiled and accessible here.
    context = std::make_unique<mc::VulkanContext>(window);
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
  std::string shadersPath = std::string(basePath ? basePath : "") + "shaders/";

  // Test loading standard shaders
  EXPECT_NO_THROW({
    mc::VulkanShader vertShader(context->GetDevice(),
                                shadersPath + "triangle.vert.spv");
    EXPECT_NE(vertShader.GetShaderModule(), VK_NULL_HANDLE);
  });
}

TEST_F(VulkanPipelineTest, CreateVulkanPipeline) {
  const char *basePath = SDL_GetBasePath();
  std::string shadersPath = std::string(basePath ? basePath : "") + "shaders/";

  mc::VulkanShader vertShader(context->GetDevice(),
                              shadersPath + "triangle.vert.spv");
  mc::VulkanShader fragShader(context->GetDevice(),
                              shadersPath + "triangle.frag.spv");

  mc::PipelineConfigInfo configInfo;
  mc::VulkanPipeline::DefaultPipelineConfigInfo(configInfo);
  configInfo.renderPass = context->GetRenderPass()->GetRenderPass();

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
