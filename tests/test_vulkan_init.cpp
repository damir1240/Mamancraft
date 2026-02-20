#include "Mamancraft/Core/Logger.hpp"
#include "Mamancraft/Renderer/VulkanContext.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <gtest/gtest.h>

using namespace mc;

class VulkanContextTest : public ::testing::Test {
protected:
  void SetUp() override {
    ASSERT_TRUE(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS));

    m_Window = SDL_CreateWindow("Test Window", 800, 600,
                                SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN);
    ASSERT_NE(m_Window, nullptr);
  }

  void TearDown() override {
    if (m_Window) {
      SDL_DestroyWindow(m_Window);
    }
    SDL_Quit();
  }

  SDL_Window *m_Window = nullptr;
};

TEST_F(VulkanContextTest, InitializationPhases) {
  std::unique_ptr<VulkanContext> context =
      std::make_unique<VulkanContext>(m_Window);

  // Phase 2: Core Vulkan Objects
  ASSERT_NE(context->GetInstance(), VK_NULL_HANDLE);
  ASSERT_NE(context->GetSurface(), VK_NULL_HANDLE);

  ASSERT_NE(context->GetDevice(), nullptr);
  EXPECT_NE(context->GetDevice()->GetPhysicalDevice(), VK_NULL_HANDLE);
  EXPECT_NE(context->GetDevice()->GetLogicalDevice(), VK_NULL_HANDLE);
  EXPECT_NE(context->GetDevice()->GetGraphicsQueue(), VK_NULL_HANDLE);
  EXPECT_NE(context->GetDevice()->GetPresentQueue(), VK_NULL_HANDLE);

  // Phase 3: Swapchain
  ASSERT_NE(context->GetSwapchain(), nullptr);
  EXPECT_NE(context->GetSwapchain()->GetSwapchain(), VK_NULL_HANDLE);
  EXPECT_GT(context->GetSwapchain()->GetImages().size(), 0);
  EXPECT_GT(context->GetSwapchain()->GetImageViews().size(), 0);

  // Phase 4: Allocator & Commands
  ASSERT_NE(context->GetAllocator(), nullptr);
  EXPECT_NE(context->GetAllocator()->GetAllocator(), VK_NULL_HANDLE);

  ASSERT_NE(context->GetCommandPool(), nullptr);
  EXPECT_NE(context->GetCommandPool()->GetCommandPool(), VK_NULL_HANDLE);

  auto cmdBuffer = context->GetCommandPool()->AllocateCommandBuffer(true);
  ASSERT_NE(cmdBuffer, nullptr);
  EXPECT_NE(cmdBuffer->GetCommandBuffer(), VK_NULL_HANDLE);
}

int main(int argc, char **argv) {
  mc::Logger::Init();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
