#pragma once

#include "Mamancraft/Renderer/Vulkan/VulkanAllocator.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanCommandPool.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanCore.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanDevice.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanSwapchain.hpp"
#include <SDL3/SDL.h>
#include <functional>
#include <memory>
#include <vector>

namespace mc {

class VulkanContext {
public:
  VulkanContext(SDL_Window *window);
  ~VulkanContext();

  VulkanContext(const VulkanContext &) = delete;
  VulkanContext &operator=(const VulkanContext &) = delete;

  vk::Instance GetInstance() const { return m_Instance; }
  vk::SurfaceKHR GetSurface() const { return m_Surface; }
  const std::unique_ptr<VulkanDevice> &GetDevice() const { return m_Device; }
  const std::unique_ptr<VulkanSwapchain> &GetSwapchain() const {
    return m_Swapchain;
  }
  const std::unique_ptr<VulkanAllocator> &GetAllocator() const {
    return m_Allocator;
  }
  const std::unique_ptr<VulkanCommandPool> &GetCommandPool() const {
    return m_CommandPool;
  }

  void ImmediateSubmit(std::function<void(vk::CommandBuffer)> &&func);

private:
  void Init();
  void Shutdown();

  void CreateInstance();
  void SetupDebugMessenger();
  void CreateSurface();
  void PickPhysicalDevice();
  QueueFamilyIndices FindQueueFamilies(vk::PhysicalDevice device);
  bool CheckDeviceExtensionSupport(vk::PhysicalDevice device);
  void CreateLogicalDevice();
  void CreateSwapchain();
  void CreateAllocator();
  void CreateCommandPool();

  bool CheckValidationLayerSupport();
  std::vector<const char *> GetRequiredExtensions();

private:
  SDL_Window *m_Window;

  vk::Instance m_Instance = nullptr;
  vk::DebugUtilsMessengerEXT m_DebugMessenger = nullptr;
  vk::SurfaceKHR m_Surface = nullptr;
  vk::PhysicalDevice m_PhysicalDevice = nullptr;

  std::unique_ptr<VulkanDevice> m_Device;
  std::unique_ptr<VulkanSwapchain> m_Swapchain;
  std::unique_ptr<VulkanAllocator> m_Allocator;
  std::unique_ptr<VulkanCommandPool> m_CommandPool;

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
