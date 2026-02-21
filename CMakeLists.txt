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

# --- Shader Compilation ---
find_program(GLSLC_EXECUTABLE NAMES glslc HINTS $ENV{VULKAN_SDK}/bin)
if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc not found! Please check VULKAN_SDK installation.")
endif()

file(GLOB_RECURSE SHADERS "assets/shaders/*.vert" "assets/shaders/*.frag")
set(SPV_FILES "")

foreach(SHADER ${SHADERS})
    get_filename_component(FILE_NAME ${SHADER} NAME)
    set(SPV_FILE "${CMAKE_BINARY_DIR}/assets/shaders/${FILE_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPV_FILE}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/assets/shaders"
        COMMAND ${GLSLC_EXECUTABLE} ${SHADER} -o ${SPV_FILE}
        DEPENDS ${SHADER}
        COMMENT "Compiling shader ${FILE_NAME} to vulkan SPIR-V"
    )
    list(APPEND SPV_FILES ${SPV_FILE})
endforeach()

add_custom_target(MamancraftShaders ALL DEPENDS ${SPV_FILES})
# --------------------------

add_library(MamancraftEngine STATIC ${MAMANCRAFT_ENGINE_SRCS})
target_include_directories(MamancraftEngine PUBLIC include)
target_include_directories(MamancraftEngine PRIVATE external) # Для доступа к коренным папкам зависимостей если нужно

target_compile_definitions(MamancraftEngine PUBLIC 
    VULKAN_HPP_NO_SETTERS
)

target_link_libraries(MamancraftEngine PUBLIC 
    Vulkan::Vulkan 
    SDL3::SDL3
    spdlog::spdlog
    glm
    GPUOpen::VulkanMemoryAllocator
)

add_executable(Mamancraft src/main.cpp)
add_dependencies(Mamancraft MamancraftShaders)
target_link_libraries(Mamancraft PRIVATE MamancraftEngine)

# Copy assets to executable directory for portable structure
add_custom_command(TARGET Mamancraft POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_BINARY_DIR}/assets"
        "$<TARGET_FILE_DIR:Mamancraft>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:SDL3::SDL3-shared>"
        "$<TARGET_FILE_DIR:Mamancraft>"
)

# GoogleTest
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP true
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

add_executable(TestEngine tests/test_vulkan_init.cpp tests/test_vulkan_pipeline.cpp)
target_link_libraries(TestEngine PRIVATE 
    MamancraftEngine
    GTest::gtest
)

# Auto-copy SDL3.dll and assets to test executables
add_custom_command(TARGET TestDeps POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:SDL3::SDL3-shared>"
        "$<TARGET_FILE_DIR:TestDeps>"
)

add_custom_command(TARGET TestEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:SDL3::SDL3-shared>"
        "$<TARGET_FILE_DIR:TestEngine>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_BINARY_DIR}/assets"
        "$<TARGET_FILE_DIR:TestEngine>/assets"
)
