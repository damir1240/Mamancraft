#include "Mamancraft/Core/Application.hpp"
#include "Mamancraft/Core/Logger.hpp"

#include <exception>

int main(int argc, char *argv[]) {
  mc::Logger::Init();
  MC_INFO("Mamancraft Engine starting...");

  try {
    mc::AppConfig config;
    config.title = "Mamancraft Voxel Engine";
    config.width = 1280;
    config.height = 720;

    mc::Application app(config);
    app.Run();
  } catch (const std::exception &e) {
    MC_CRITICAL("Fatal Error: {0}", e.what());
    return -1;
  }

  MC_INFO("Mamancraft Engine shutdown gracefully.");
  return 0;
}
