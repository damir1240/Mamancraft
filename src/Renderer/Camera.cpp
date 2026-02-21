#include "Mamancraft/Renderer/Camera.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace mc {

Camera::Camera() {
  m_Projection = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
  m_View = glm::mat4(1.0f);
}

void Camera::SetPerspective(float fov, float aspect, float znear, float zfar) {
  m_Projection = glm::perspective(fov, aspect, znear, zfar);
  // Vulkan projection matrix has Y inverted compared to OpenGL
  m_Projection[1][1] *= -1.0f;
}

void Camera::SetOrthographic(float left, float right, float bottom, float top,
                             float znear, float zfar) {
  m_Projection = glm::ortho(left, right, bottom, top, znear, zfar);
  m_Projection[1][1] *= -1.0f;
}

void Camera::Update() {
  glm::mat4 rotation = glm::mat4(1.0f);
  rotation =
      glm::rotate(rotation, glm::radians(m_Rotation.x), {1.0f, 0.0f, 0.0f});
  rotation =
      glm::rotate(rotation, glm::radians(m_Rotation.y), {0.0f, 1.0f, 0.0f});
  rotation =
      glm::rotate(rotation, glm::radians(m_Rotation.z), {0.0f, 0.0f, 1.0f});

  glm::vec3 translation = -m_Position;
  m_View = rotation * glm::translate(glm::mat4(1.0f), translation);
}

} // namespace mc
