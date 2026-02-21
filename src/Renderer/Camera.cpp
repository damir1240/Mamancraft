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
  float pitch = glm::radians(m_Rotation.x);
  float yaw = glm::radians(m_Rotation.y);

  m_Forward.x = sin(yaw) * cos(pitch);
  m_Forward.y = sin(pitch);
  m_Forward.z = -cos(yaw) * cos(pitch);
  m_Forward = glm::normalize(m_Forward);

  m_Right = glm::normalize(glm::cross(m_Forward, glm::vec3(0.0f, 1.0f, 0.0f)));
  m_Up = glm::normalize(glm::cross(m_Right, m_Forward));

  m_View = glm::lookAt(m_Position, m_Position + m_Forward, m_Up);
}

} // namespace mc
