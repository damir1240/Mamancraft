#include "Mamancraft/Core/InputManager.hpp"
#include "Mamancraft/Core/Logger.hpp"

namespace mc {

InputManager::InputManager() {
  MC_INFO("InputManager initialized (SDL3 Hybrid Model)");
}

void InputManager::NewFrame() {
  // Copy current states to previous for transition detection
  m_KeysPrevious = m_KeysCurrent;
  m_MouseButtonsPrevious = m_MouseButtonsCurrent;

  // Reset frame-based deltas
  m_MouseDelta = {0.0f, 0.0f};
  m_MouseScroll = 0.0f;
}

void InputManager::HandleEvent(const SDL_Event &event) {
  switch (event.type) {
  case SDL_EVENT_KEY_DOWN:
    m_KeysCurrent[event.key.scancode] = true;
    break;
  case SDL_EVENT_KEY_UP:
    m_KeysCurrent[event.key.scancode] = false;
    break;

  case SDL_EVENT_MOUSE_BUTTON_DOWN:
    m_MouseButtonsCurrent[event.button.button] = true;
    break;
  case SDL_EVENT_MOUSE_BUTTON_UP:
    m_MouseButtonsCurrent[event.button.button] = false;
    break;

  case SDL_EVENT_MOUSE_MOTION:
    m_MousePos = {event.motion.x, event.motion.y};
    m_MouseDelta += glm::vec2{event.motion.xrel, event.motion.yrel};
    break;

  case SDL_EVENT_MOUSE_WHEEL:
    m_MouseScroll += event.wheel.y; // Standard scroll
    break;

  default:
    break;
  }
}

void InputManager::SetCursorLocking(SDL_Window *window, bool locked) {
  if (locked) {
    SDL_SetWindowRelativeMouseMode(window, true);
    MC_INFO("Cursor locked for first-person controls.");
  } else {
    SDL_SetWindowRelativeMouseMode(window, false);
    MC_INFO("Cursor released.");
  }
  m_IsCursorLocked = locked;
}

// --- Keyboard ---
bool InputManager::IsKeyPressed(SDL_Scancode key) const {
  auto curIt = m_KeysCurrent.find(key);
  auto prevIt = m_KeysPrevious.find(key);

  bool current = (curIt != m_KeysCurrent.end()) ? curIt->second : false;
  bool previous = (prevIt != m_KeysPrevious.end()) ? prevIt->second : false;

  return current && !previous;
}

bool InputManager::IsKeyHeld(SDL_Scancode key) const {
  auto curIt = m_KeysCurrent.find(key);
  return (curIt != m_KeysCurrent.end()) ? curIt->second : false;
}

bool InputManager::IsKeyReleased(SDL_Scancode key) const {
  auto curIt = m_KeysCurrent.find(key);
  auto prevIt = m_KeysPrevious.find(key);

  bool current = (curIt != m_KeysCurrent.end()) ? curIt->second : false;
  bool previous = (prevIt != m_KeysPrevious.end()) ? prevIt->second : false;

  return !current && previous;
}

// --- Mouse ---
bool InputManager::IsMouseButtonPressed(uint8_t button) const {
  auto curIt = m_MouseButtonsCurrent.find(button);
  auto prevIt = m_MouseButtonsPrevious.find(button);

  bool current = (curIt != m_MouseButtonsCurrent.end()) ? curIt->second : false;
  bool previous =
      (prevIt != m_MouseButtonsPrevious.end()) ? prevIt->second : false;

  return current && !previous;
}

bool InputManager::IsMouseButtonHeld(uint8_t button) const {
  auto curIt = m_MouseButtonsCurrent.find(button);
  return (curIt != m_MouseButtonsCurrent.end()) ? curIt->second : false;
}

bool InputManager::IsMouseButtonReleased(uint8_t button) const {
  auto curIt = m_MouseButtonsCurrent.find(button);
  auto prevIt = m_MouseButtonsPrevious.find(button);

  bool current = (curIt != m_MouseButtonsCurrent.end()) ? curIt->second : false;
  bool previous =
      (prevIt != m_MouseButtonsPrevious.end()) ? prevIt->second : false;

  return !current && previous;
}

// --- Action Mapping ---
void InputManager::BindAction(const std::string &actionName, SDL_Scancode key) {
  m_ActionMap[actionName].keys.push_back(key);
}

void InputManager::BindMouseButton(const std::string &actionName,
                                   uint8_t button) {
  m_ActionMap[actionName].mouseButtons.push_back(button);
}

bool InputManager::IsActionPressed(const std::string &actionName) const {
  if (!m_ActionMap.contains(actionName))
    return false;
  const auto &binding = m_ActionMap.at(actionName);

  for (auto key : binding.keys)
    if (IsKeyPressed(key))
      return true;
  for (auto btn : binding.mouseButtons)
    if (IsMouseButtonPressed(btn))
      return true;

  return false;
}

bool InputManager::IsActionHeld(const std::string &actionName) const {
  if (!m_ActionMap.contains(actionName))
    return false;
  const auto &binding = m_ActionMap.at(actionName);

  for (auto key : binding.keys)
    if (IsKeyHeld(key))
      return true;
  for (auto btn : binding.mouseButtons)
    if (IsMouseButtonHeld(btn))
      return true;

  return false;
}

bool InputManager::IsActionReleased(const std::string &actionName) const {
  if (!m_ActionMap.contains(actionName))
    return false;
  const auto &binding = m_ActionMap.at(actionName);

  for (auto key : binding.keys)
    if (IsKeyReleased(key))
      return true;
  for (auto btn : binding.mouseButtons)
    if (IsMouseButtonReleased(btn))
      return true;

  return false;
}

} // namespace mc
