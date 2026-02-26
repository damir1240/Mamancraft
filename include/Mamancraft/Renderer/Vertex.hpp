#pragma once

#include "Mamancraft/Renderer/Vulkan/VulkanCore.hpp"
#include <array>
#include <cstdint>
#include <glm/glm.hpp>

namespace mc {

/// Compact GPU-Driven vertex format (20 bytes).
///
/// Layout:
///   pos    — vec3  (12 bytes) world-space position
///   packed — uint  ( 4 bytes) bits [0..15] = materialID, bits [16..18] =
///   normal index (0-5) uv     — vec2  ( 8 bytes) texture coordinates (greedy
///   mesh needs full precision)
///
/// Normal index encoding:
///   0 = +Y (Top),    1 = -Y (Bottom)
///   2 = +Z (Front),  3 = -Z (Back)
///   4 = +X (Right),  5 = -X (Left)
struct Vertex {
  glm::vec3 pos;
  uint32_t packed; // materialID (16 bit) | normalIndex (3 bit) << 16
  glm::vec2 uv;

  /// Pack material ID and normal index into a single uint32_t.
  static constexpr uint32_t Pack(uint32_t materialID, uint32_t normalIndex) {
    return (materialID & 0xFFFFu) | ((normalIndex & 0x7u) << 16u);
  }

  /// Extract material ID from packed field.
  static constexpr uint32_t UnpackMaterialID(uint32_t packed) {
    return packed & 0xFFFFu;
  }

  /// Extract normal index from packed field.
  static constexpr uint32_t UnpackNormalIndex(uint32_t packed) {
    return (packed >> 16u) & 0x7u;
  }

  static vk::VertexInputBindingDescription GetBindingDescription() {
    vk::VertexInputBindingDescription bindingDescription;
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = vk::VertexInputRate::eVertex;
    return bindingDescription;
  }

  static std::array<vk::VertexInputAttributeDescription, 3>
  GetAttributeDescriptions() {
    std::array<vk::VertexInputAttributeDescription, 3> attributeDescriptions;

    // location 0: position (vec3)
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);

    // location 1: packed (uint32)
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = vk::Format::eR32Uint;
    attributeDescriptions[1].offset = offsetof(Vertex, packed);

    // location 2: uv (vec2)
    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = vk::Format::eR32G32Sfloat;
    attributeDescriptions[2].offset = offsetof(Vertex, uv);

    return attributeDescriptions;
  }
};

} // namespace mc
