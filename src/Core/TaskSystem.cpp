#include "Mamancraft/Core/TaskSystem.hpp"
#include "Mamancraft/Core/Logger.hpp"

namespace mc {

TaskSystem::TaskSystem(uint32_t threadCount) {
  MC_INFO("TaskSystem initialized with {0} threads", threadCount);

  for (uint32_t i = 0; i < threadCount; ++i) {
    m_Workers.emplace_back([this](std::stop_token stopToken) {
      while (!stopToken.stop_requested()) {
        std::function<void()> task;
        {
          std::unique_lock<std::mutex> lock(m_QueueMutex);
          m_Condition.wait(lock, [this, &stopToken] {
            return m_Stop || !m_Tasks.empty() || stopToken.stop_requested();
          });

          if ((m_Stop || stopToken.stop_requested()) && m_Tasks.empty()) {
            return;
          }

          task = std::move(m_Tasks.front());
          m_Tasks.pop();
        }

        if (task) {
          task();
        }
      }
    });
  }
}

TaskSystem::~TaskSystem() {
  {
    std::unique_lock<std::mutex> lock(m_QueueMutex);
    m_Stop = true;
  }
  m_Condition.notify_all();
  // jthreads will automatically join on destruction
}

} // namespace mc
