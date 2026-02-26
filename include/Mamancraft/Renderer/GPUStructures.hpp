#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace mc {

/// GPU-side material descriptor. Lives in SSBO (set 0, binding 2).
/// Matches the GLSL `MaterialData` struct exactly (std430 layout).
struct alignas(16) MaterialData {
  glm::vec4 albedoTint{1.0f};    // RGBA tint color
  uint32_t albedoTexIndex = 0;   // index into bindless sampler2D array
  uint32_t animFrames = 1;       // 1 = static, N > 1 = animated strip
  float animFPS = 8.0f;          // animation speed (frames per second)
  uint32_t flags = 0;            // bitfield: bit 0 = transparent
};

/// GPU-side per-object (per-chunk) data. Lives in SSBO (set 0, binding 1).
/// Accessed in vertex shader via `objects[gl_BaseInstance]`.
struct ObjectData {
  glm::mat4 model{1.0f};        // model-to-world transform
  glm::vec4 aabbMin{0.0f};      // world-space AABB for frustum culling
  glm::vec4 aabbMax{0.0f};
};

/// Maps directly to VkDrawIndexedIndirectCommand.
/// Lives in SSBO used by vkCmdDrawIndexedIndirect.
struct DrawCommand {
  uint32_t indexCount = 0;
  uint32_t instanceCount = 1;    // 0 = culled by compute, 1 = visible
  uint32_t firstIndex = 0;
  int32_t vertexOffset = 0;
  uint32_t firstInstance = 0;    // == drawID, used as gl_BaseInstance
};

/// Uniform data for frustum culling compute shader.
struct CullUniforms {
  glm::mat4 viewProj{1.0f};
  glm::vec4 frustumPlanes[6]{};
  uint32_t drawCount = 0;
  uint32_t _pad0 = 0;
  uint32_t _pad1 = 0;
  uint32_t _pad2 = 0;
};

} // namespace mc
