#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>
#include <cassert>

void test_spdlog() {
    spdlog::info("Testing spdlog...");
    // If it compiles and runs, it's working
}

void test_glm() {
    glm::vec4 v(1.0f);
    glm::mat4 m(1.0f);
    auto res = m * v;
    assert(res.x == 1.0f);
    spdlog::info("GLM test passed.");
}

void test_sdl() {
    bool success = SDL_Init(SDL_INIT_VIDEO);
    assert(success);
    SDL_Quit();
    spdlog::info("SDL3 test passed.");
}

void test_vulkan() {
    uint32_t count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    spdlog::info("Vulkan test passed: found {} extensions.", count);
}

int main() {
    spdlog::set_level(spdlog::level::debug);
    test_spdlog();
    test_glm();
    test_sdl();
    test_vulkan();
    spdlog::info("All dependency tests passed!");
    return 0;
}
