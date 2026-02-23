#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>


namespace mc {

/**
 * @brief A high-performance thread pool for executing tasks across all CPU
 * cores. Follows C++20 best practices using std::jthread and modern
 * synchronization.
 */
class TaskSystem {
public:
  TaskSystem(uint32_t threadCount = std::thread::hardware_concurrency());
  ~TaskSystem();

  // Prevent copying
  TaskSystem(const TaskSystem &) = delete;
  TaskSystem &operator=(const TaskSystem &) = delete;

  /**
   * @brief Enqueue a task for execution. Returns a future for the result.
   */
  template <class F, class... Args>
  auto Enqueue(F &&f, Args &&...args)
      -> std::future<typename std::invoke_result_t<F, Args...>> {
    using return_type = typename std::invoke_result_t<F, Args...>;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    std::future<return_type> res = task->get_future();
    {
      std::unique_lock<std::mutex> lock(m_QueueMutex);
      if (m_Stop) {
        throw std::runtime_error("Enqueue on stopped TaskSystem");
      }
      m_Tasks.emplace([task]() { (*task)(); });
    }
    m_Condition.notify_one();
    return res;
  }

  uint32_t GetThreadCount() const {
    return static_cast<uint32_t>(m_Workers.size());
  }

private:
  std::vector<std::jthread> m_Workers;
  std::queue<std::function<void()>> m_Tasks;

  std::mutex m_QueueMutex;
  std::condition_variable m_Condition;
  std::atomic<bool> m_Stop{false};
};

} // namespace mc
