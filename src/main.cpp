#include <iostream>
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

int main(int argc, char* argv[]) {
    spdlog::info("Starting Mamancraft Engine...");
    
    // Minimal SDL3 Init
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        spdlog::error("SDL_Init Error: {}", SDL_GetError());
        return -1;
    }
    spdlog::info("SDL3 initialized successfully.");

    // Minimal Vulkan Version Check
    uint32_t apiVersion;
    if (vkEnumerateInstanceVersion(&apiVersion) == VK_SUCCESS) {
        spdlog::info("Vulkan Instance Version: {}.{}.{}", 
            VK_API_VERSION_MAJOR(apiVersion),
            VK_API_VERSION_MINOR(apiVersion),
            VK_API_VERSION_PATCH(apiVersion));
    }

    // Minimal GLM check
    glm::vec3 testVec(1.0f, 2.0f, 3.0f);
    spdlog::info("GLM Test Vector: ({}, {}, {})", testVec.x, testVec.y, testVec.z);

    SDL_Quit();
    spdlog::info("Mamancraft Engine shut down.");
    return 0;
}
