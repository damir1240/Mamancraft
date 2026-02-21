#pragma once

#include <deque>
#include <functional>

namespace mc {

/**
 * @brief Utility for deferred destruction of Vulkan resources.
 *
 * In Vulkan, you cannot destroy resources that are currently in use by the GPU.
 * A DeletionQueue allows you to queue up destruction tasks to be executed
 * later, typically at the end of a frame or when the engine shuts down.
 */
class DeletionQueue {
public:
  void PushFunction(std::function<void()> &&function) {
    m_Deletors.push_back(std::move(function));
  }

  void Flush() {
    for (auto it = m_Deletors.rbegin(); it != m_Deletors.rend(); ++it) {
      (*it)();
    }
    m_Deletors.clear();
  }

private:
  std::deque<std::function<void()>> m_Deletors;
};

} // namespace mc
