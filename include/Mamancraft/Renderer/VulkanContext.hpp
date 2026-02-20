#pragma once

#include <SDL3/SDL.h>
#include <vector>
#include <vulkan/vulkan.h>


namespace mc {

class VulkanContext {
public:
  VulkanContext(SDL_Window *window);
  ~VulkanContext();

  VulkanContext(const VulkanContext &) = delete;
  VulkanContext &operator=(const VulkanContext &) = delete;

  VkInstance GetInstance() const { return m_Instance; }
  VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
  VkDevice GetDevice() const { return m_Device; }

private:
  void Init();
  void Shutdown();

  void CreateInstance();
  void SetupDebugMessenger();
  void PickPhysicalDevice();
  void CreateLogicalDevice();

  bool CheckValidationLayerSupport();
  std::vector<const char *> GetRequiredExtensions();

private:
  SDL_Window *m_Window;

  VkInstance m_Instance = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
  VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
  VkDevice m_Device = VK_NULL_HANDLE;
  VkQueue m_GraphicsQueue = VK_NULL_HANDLE;

#ifdef NDEBUG
  const bool m_EnableValidationLayers = false;
#else
  const bool m_EnableValidationLayers = true;
#endif

  const std::vector<const char *> m_ValidationLayers = {
      "VK_LAYER_KHRONOS_validation"};
};

} // namespace mc
