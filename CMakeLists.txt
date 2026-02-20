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

# Main Executable
add_executable(Mamancraft src/main.cpp)
target_link_libraries(Mamancraft PRIVATE 
    Vulkan::Vulkan 
    SDL3::SDL3 
    spdlog::spdlog
    glm
)

# Test Executable
add_executable(TestDeps tests/test_deps.cpp)
target_link_libraries(TestDeps PRIVATE 
    Vulkan::Vulkan 
    SDL3::SDL3 
    spdlog::spdlog
    glm
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
