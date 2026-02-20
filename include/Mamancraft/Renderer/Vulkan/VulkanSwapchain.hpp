#pragma once

#include "Mamancraft/Renderer/Vulkan/VulkanDevice.hpp"
#include <SDL3/SDL.h>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>


namespace mc {

struct SwapchainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
};

class VulkanSwapchain {
public:
  VulkanSwapchain(const std::unique_ptr<VulkanDevice> &device,
                  VkInstance instance, VkSurfaceKHR surface,
                  SDL_Window *window);
  ~VulkanSwapchain();

  VulkanSwapchain(const VulkanSwapchain &) = delete;
  VulkanSwapchain &operator=(const VulkanSwapchain &) = delete;

  void Recreate();

  VkSwapchainKHR GetSwapchain() const { return m_Swapchain; }
  VkFormat GetImageFormat() const { return m_ImageFormat; }
  VkExtent2D GetExtent() const { return m_Extent; }
  const std::vector<VkImage> &GetImages() const { return m_Images; }
  const std::vector<VkImageView> &GetImageViews() const { return m_ImageViews; }

  static SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice device,
                                                       VkSurfaceKHR surface);

private:
  void CreateSwapchain();
  void CreateImageViews();
  void CleanUp();

  VkSurfaceFormatKHR ChooseSwapSurfaceFormat(
      const std::vector<VkSurfaceFormatKHR> &availableFormats);
  VkPresentModeKHR ChooseSwapPresentMode(
      const std::vector<VkPresentModeKHR> &availablePresentModes);
  VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

private:
  const std::unique_ptr<VulkanDevice> &m_Device;
  VkInstance m_Instance;
  VkSurfaceKHR m_Surface;
  SDL_Window *m_Window;

  VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
  VkFormat m_ImageFormat;
  VkExtent2D m_Extent;

  std::vector<VkImage> m_Images;
  std::vector<VkImageView> m_ImageViews;
};

} // namespace mc
