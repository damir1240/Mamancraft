cmake_minimum_required(VERSION 3.20)
project(Mamancraft LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Vulkan
find_package(Vulkan 1.4 REQUIRED)

# Add dependencies from submodules
add_subdirectory(external/SDL EXCLUDE_FROM_ALL)
add_subdirectory(external/spdlog EXCLUDE_FROM_ALL)

# GLM is header-only
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE external/glm)

# VMA Submodule
add_subdirectory(external/vma EXCLUDE_FROM_ALL)

# Main Executable & Library
file(GLOB_RECURSE MAMANCRAFT_ENGINE_SRCS "src/*.cpp")
list(REMOVE_ITEM MAMANCRAFT_ENGINE_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

add_library(MamancraftEngine STATIC ${MAMANCRAFT_ENGINE_SRCS})
target_include_directories(MamancraftEngine PUBLIC include)
target_include_directories(MamancraftEngine PRIVATE external) # Для доступа к коренным папкам зависимостей если нужно

target_link_libraries(MamancraftEngine PUBLIC 
    Vulkan::Vulkan 
    SDL3::SDL3
    spdlog::spdlog
    glm
    GPUOpen::VulkanMemoryAllocator
)

add_executable(Mamancraft src/main.cpp)
target_link_libraries(Mamancraft PRIVATE MamancraftEngine)

# GoogleTest
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# For Windows: Prevent overriding the parent project's linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Test Executable
add_executable(TestDeps tests/test_deps.cpp)
target_link_libraries(TestDeps PRIVATE 
    Vulkan::Vulkan 
    SDL3::SDL3 
    spdlog::spdlog
    glm
    GPUOpen::VulkanMemoryAllocator
)

add_executable(TestEngine tests/test_vulkan_init.cpp)
target_link_libraries(TestEngine PRIVATE 
    MamancraftEngine
    GTest::gtest
)

# Auto-copy SDL3.dll to the output directories
add_custom_command(TARGET Mamancraft POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:SDL3::SDL3-shared>"
    "$<TARGET_FILE_DIR:Mamancraft>"
)

add_custom_command(TARGET TestDeps POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:SDL3::SDL3-shared>"
    "$<TARGET_FILE_DIR:TestDeps>"
)

add_custom_command(TARGET TestEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:SDL3::SDL3-shared>"
    "$<TARGET_FILE_DIR:TestEngine>"
)
