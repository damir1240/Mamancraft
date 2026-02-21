#pragma once

#include <memory>
#include <spdlog/spdlog.h>

namespace mc {

class Logger {
public:
  static void Init(const std::string &logFilePath = "Mamancraft.log");

  static std::shared_ptr<spdlog::logger> GetCoreLogger();

private:
  static std::shared_ptr<spdlog::logger> s_CoreLogger;
};

} // namespace mc

// Core log macros
#define MC_TRACE(...)                                                          \
  if (::mc::Logger::GetCoreLogger())                                           \
  ::mc::Logger::GetCoreLogger()->trace(__VA_ARGS__)
#define MC_DEBUG(...)                                                          \
  if (::mc::Logger::GetCoreLogger())                                           \
  ::mc::Logger::GetCoreLogger()->debug(__VA_ARGS__)
#define MC_INFO(...)                                                           \
  if (::mc::Logger::GetCoreLogger())                                           \
  ::mc::Logger::GetCoreLogger()->info(__VA_ARGS__)
#define MC_WARN(...)                                                           \
  if (::mc::Logger::GetCoreLogger())                                           \
  ::mc::Logger::GetCoreLogger()->warn(__VA_ARGS__)
#define MC_ERROR(...)                                                          \
  if (::mc::Logger::GetCoreLogger())                                           \
  ::mc::Logger::GetCoreLogger()->error(__VA_ARGS__)
#define MC_CRITICAL(...)                                                       \
  if (::mc::Logger::GetCoreLogger())                                           \
  ::mc::Logger::GetCoreLogger()->critical(__VA_ARGS__)
