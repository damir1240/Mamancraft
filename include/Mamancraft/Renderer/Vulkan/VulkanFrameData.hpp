#pragma once

#include <glm/glm.hpp>

namespace mc {

struct GlobalUbo {
  glm::mat4 projection{1.0f};
  glm::mat4 view{1.0f};
  glm::mat4 inverseView{1.0f};
  float time{0.0f};  // elapsed time in seconds (for animated textures)
  float _pad0{0.0f}; // align to 16 bytes
  float _pad1{0.0f};
  float _pad2{0.0f};
};

// PushConstantData is no longer used in GPU-Driven paradigm.
// Per-object transforms are stored in ObjectData SSBO (see GPUStructures.hpp).
// This struct is retained for backward compatibility with tests.
struct PushConstantData {
  glm::mat4 model{1.0f};
};

} // namespace mc
