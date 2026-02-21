#pragma once

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace mc {

/**
 * @brief Represents the state of a button or key.
 */
enum class InputState { None = 0, Pressed, Held, Released };

/**
 * @brief Modern Input System for Mamancraft.
 *
 * Best Practices:
 * - State tracking (Pressed/Held/Released Transitions).
 * - Action Mapping (Logical names -> Physical keys).
 * - Hybrid Event/Polling model.
 * - Mouse delta and scroll tracking.
 */
class InputManager {
public:
  InputManager();
  ~InputManager() = default;

  // Prevent copying
  InputManager(const InputManager &) = delete;
  InputManager &operator=(const InputManager &) = delete;

  /**
   * @brief Must be called at the start of each frame to reset per-frame deltas.
   */
  void NewFrame();

  /**
   * @brief Processes SDL events and updates internal raw states.
   */
  void HandleEvent(const SDL_Event &event);

  // --- Keyboard Polling ---
  bool IsKeyPressed(SDL_Scancode key) const;
  bool IsKeyHeld(SDL_Scancode key) const;
  bool IsKeyReleased(SDL_Scancode key) const;

  // --- Mouse Polling ---
  bool IsMouseButtonPressed(uint8_t button) const;
  bool IsMouseButtonHeld(uint8_t button) const;
  bool IsMouseButtonReleased(uint8_t button) const;

  glm::vec2 GetMousePosition() const { return m_MousePos; }
  glm::vec2 GetMouseDelta() const { return m_MouseDelta; }
  float GetMouseScroll() const { return m_MouseScroll; }

  void SetCursorLocking(SDL_Window *window, bool locked);
  bool IsCursorLocked() const { return m_IsCursorLocked; }

  // --- Action Mapping ---
  void BindAction(const std::string &actionName, SDL_Scancode key);
  void BindMouseButton(const std::string &actionName, uint8_t button);
  void ClearBindings(const std::string &actionName);

  bool IsActionPressed(const std::string &actionName) const;
  bool IsActionHeld(const std::string &actionName) const;
  bool IsActionReleased(const std::string &actionName) const;

  // --- Configuration ---
  void LoadConfiguration(const std::string &path);
  void SaveConfiguration(const std::string &path);

private:
  // Raw Keyboard State
  std::unordered_map<SDL_Scancode, bool> m_KeysCurrent;
  std::unordered_map<SDL_Scancode, bool> m_KeysPrevious;

  // Raw Mouse State
  std::unordered_map<uint8_t, bool> m_MouseButtonsCurrent;
  std::unordered_map<uint8_t, bool> m_MouseButtonsPrevious;

  glm::vec2 m_MousePos{0.0f};
  glm::vec2 m_MouseDelta{0.0f};
  float m_MouseScroll = 0.0f;
  bool m_IsCursorLocked = false;

  // Action Maps
  struct ActionBinding {
    std::vector<SDL_Scancode> keys;
    std::vector<uint8_t> mouseButtons;
  };
  std::unordered_map<std::string, ActionBinding> m_ActionMap;
};

} // namespace mc
