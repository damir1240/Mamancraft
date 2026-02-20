#pragma once

#include "Mamancraft/Renderer/Vulkan/VulkanDevice.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanSwapchain.hpp"
#include <SDL3/SDL.h>
#include <memory>
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
  VkSurfaceKHR GetSurface() const { return m_Surface; }
  const std::unique_ptr<VulkanDevice> &GetDevice() const { return m_Device; }
  const std::unique_ptr<VulkanSwapchain> &GetSwapchain() const {
    return m_Swapchain;
  }

private:
  void Init();
  void Shutdown();

  void CreateInstance();
  void SetupDebugMessenger();
  void CreateSurface();
  void PickPhysicalDevice();
  QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
  bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
  void CreateLogicalDevice();
  void CreateSwapchain();

  bool CheckValidationLayerSupport();
  std::vector<const char *> GetRequiredExtensions();

private:
  SDL_Window *m_Window;

  VkInstance m_Instance = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
  VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
  VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;

  std::unique_ptr<VulkanDevice> m_Device;
  std::unique_ptr<VulkanSwapchain> m_Swapchain;

  const std::vector<const char *> m_DeviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#ifdef NDEBUG
  const bool m_EnableValidationLayers = false;
#else
  const bool m_EnableValidationLayers = true;
#endif

  const std::vector<const char *> m_ValidationLayers = {
      "VK_LAYER_KHRONOS_validation"};
};

} // namespace mc
