#pragma once

#include <memory>
#include <spdlog/spdlog.h>

namespace mc {

class Logger {
public:
  static void Init(const std::string &logFilePath = "Mamancraft.log");

  inline static std::shared_ptr<spdlog::logger> &GetCoreLogger() {
    return s_CoreLogger;
  }

private:
  static std::shared_ptr<spdlog::logger> s_CoreLogger;
};

} // namespace mc

// Core log macros
#define MC_TRACE(...) ::mc::Logger::GetCoreLogger()->trace(__VA_ARGS__)
#define MC_DEBUG(...) ::mc::Logger::GetCoreLogger()->debug(__VA_ARGS__)
#define MC_INFO(...) ::mc::Logger::GetCoreLogger()->info(__VA_ARGS__)
#define MC_WARN(...) ::mc::Logger::GetCoreLogger()->warn(__VA_ARGS__)
#define MC_ERROR(...) ::mc::Logger::GetCoreLogger()->error(__VA_ARGS__)
#define MC_CRITICAL(...) ::mc::Logger::GetCoreLogger()->critical(__VA_ARGS__)
