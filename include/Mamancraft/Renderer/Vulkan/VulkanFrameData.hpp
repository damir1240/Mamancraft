#pragma once

#include <glm/glm.hpp>

namespace mc {

struct GlobalUbo {
  glm::mat4 projection{1.0f};
  glm::mat4 view{1.0f};
  glm::mat4 inverseView{1.0f};
};

struct PushConstantData {
  glm::mat4 model{1.0f};
};

} // namespace mc
