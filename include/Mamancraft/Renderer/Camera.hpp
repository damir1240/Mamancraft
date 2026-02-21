#pragma once

#include <glm/glm.hpp>

namespace mc {

class Camera {
public:
  Camera();
  ~Camera() = default;

  void SetPerspective(float fov, float aspect, float znear, float zfar);
  void SetOrthographic(float left, float right, float bottom, float top,
                       float znear, float zfar);

  void SetPosition(const glm::vec3 &position) { m_Position = position; }
  void SetRotation(const glm::vec3 &rotation) { m_Rotation = rotation; }

  const glm::vec3 &GetPosition() const { return m_Position; }
  const glm::vec3 &GetRotation() const { return m_Rotation; }

  const glm::mat4 &GetProjection() const { return m_Projection; }
  const glm::mat4 &GetView() const { return m_View; }

  glm::vec3 GetForward() const { return m_Forward; }
  glm::vec3 GetRight() const { return m_Right; }
  glm::vec3 GetUp() const { return m_Up; }

  void Update();

private:
  glm::mat4 m_Projection{1.0f};
  glm::mat4 m_View{1.0f};

  glm::vec3 m_Position{0.0f, 0.0f, 0.0f};
  glm::vec3 m_Rotation{0.0f, 0.0f, 0.0f}; // Pitch, Yaw, Roll

  glm::vec3 m_Forward{0.0f, 0.0f, -1.0f};
  glm::vec3 m_Right{1.0f, 0.0f, 0.0f};
  glm::vec3 m_Up{0.0f, 1.0f, 0.0f};
};

} // namespace mc
