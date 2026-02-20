#pragma once

#include <SDL3/SDL.h>
#include <cstdint>
#include <string>


namespace mc {

struct AppConfig {
  std::string title = "Mamancraft";
  uint32_t width = 1280;
  uint32_t height = 720;
};

class Application {
public:
  Application(const AppConfig &config);
  ~Application();

  // Prevent copying
  Application(const Application &) = delete;
  Application &operator=(const Application &) = delete;

  void Run();

private:
  void Init();
  void Shutdown();
  void ProcessEvents();
  void Update();

private:
  AppConfig m_Config;
  bool m_IsRunning = false;
  SDL_Window *m_Window = nullptr;
};

} // namespace mc
