#include "Mamancraft/Core/InputManager.hpp"
#include "Mamancraft/Core/Logger.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>


namespace mc {

// Helper: Trim whitespace from string
static std::string Trim(const std::string &s) {
  auto start = s.find_first_not_of(" \t\n\r");
  auto end = s.find_last_not_of(" \t\n\r");
  return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

// Helper: Mouse button mapping
static std::string MouseButtonName(uint8_t button) {
  switch (button) {
  case SDL_BUTTON_LEFT:
    return "MouseLeft";
  case SDL_BUTTON_MIDDLE:
    return "MouseMiddle";
  case SDL_BUTTON_RIGHT:
    return "MouseRight";
  case SDL_BUTTON_X1:
    return "MouseX1";
  case SDL_BUTTON_X2:
    return "MouseX2";
  default:
    return "UnknownMouse";
  }
}

static uint8_t MouseButtonFromName(const std::string &name) {
  if (name == "MouseLeft")
    return SDL_BUTTON_LEFT;
  if (name == "MouseMiddle")
    return SDL_BUTTON_MIDDLE;
  if (name == "MouseRight")
    return SDL_BUTTON_RIGHT;
  if (name == "MouseX1")
    return SDL_BUTTON_X1;
  if (name == "MouseX2")
    return SDL_BUTTON_X2;
  return 0;
}

InputManager::InputManager() {
  MC_INFO("InputManager initialized (SDL3 Configurable Model)");
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
    if (!event.key.repeat) {
      m_KeysCurrent[event.key.scancode] = true;
    }
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
  auto &mapping = m_ActionMap[actionName];
  if (std::find(mapping.keys.begin(), mapping.keys.end(), key) ==
      mapping.keys.end()) {
    mapping.keys.push_back(key);
  }
}

void InputManager::BindMouseButton(const std::string &actionName,
                                   uint8_t button) {
  auto &mapping = m_ActionMap[actionName];
  if (std::find(mapping.mouseButtons.begin(), mapping.mouseButtons.end(),
                button) == mapping.mouseButtons.end()) {
    mapping.mouseButtons.push_back(button);
  }
}

void InputManager::ClearBindings(const std::string &actionName) {
  m_ActionMap.erase(actionName);
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

// --- Configuration ---
void InputManager::LoadConfiguration(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    MC_WARN("Could not open input config: {}. Using default/current bindings.",
            path);
    return;
  }

  std::string line;
  while (std::getline(file, line)) {
    // Simple format: ActionName = Key1, Key2, MouseButton1
    size_t sep = line.find('=');
    if (sep == std::string::npos)
      continue;

    std::string actionName = Trim(line.substr(0, sep));
    std::string bindingsPart = line.substr(sep + 1);

    std::stringstream ss(bindingsPart);
    std::string binding;

    ClearBindings(actionName); // Reset before loading new

    while (std::getline(ss, binding, ',')) {
      std::string name = Trim(binding);
      if (name.empty())
        continue;

      // Try mouse button
      uint8_t mouseBtn = MouseButtonFromName(name);
      if (mouseBtn != 0) {
        BindMouseButton(actionName, mouseBtn);
      } else {
        // Try scancode
        SDL_Scancode sc = SDL_GetScancodeFromName(name.c_str());
        if (sc != SDL_SCANCODE_UNKNOWN) {
          BindAction(actionName, sc);
        } else {
          MC_WARN("Unknown binding '{}' for action '{}'", name, actionName);
        }
      }
    }
  }
  MC_INFO("Input configuration loaded from {}", path);
}

void InputManager::SaveConfiguration(const std::string &path) {
  std::ofstream file(path);
  if (!file.is_open()) {
    MC_ERROR("Could not open file for saving input config: {}", path);
    return;
  }

  for (const auto &[actionName, binding] : m_ActionMap) {
    file << actionName << " = ";
    bool first = true;

    for (auto key : binding.keys) {
      if (!first)
        file << ", ";
      file << SDL_GetScancodeName(key);
      first = false;
    }

    for (auto btn : binding.mouseButtons) {
      if (!first)
        file << ", ";
      file << MouseButtonName(btn);
      first = false;
    }
    file << "\n";
  }
  MC_INFO("Input configuration saved to {}", path);
}

} // namespace mc
