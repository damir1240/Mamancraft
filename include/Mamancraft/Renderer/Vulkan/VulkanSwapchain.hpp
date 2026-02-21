#pragma once

#include "Mamancraft/Renderer/Vulkan/VulkanCore.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanDevice.hpp"
#include <SDL3/SDL.h>
#include <memory>
#include <vector>

namespace mc {

struct SwapchainSupportDetails {
  vk::SurfaceCapabilitiesKHR capabilities;
  std::vector<vk::SurfaceFormatKHR> formats;
  std::vector<vk::PresentModeKHR> presentModes;
};

class VulkanSwapchain {
public:
  VulkanSwapchain(const std::unique_ptr<VulkanDevice> &device,
                  vk::Instance instance, vk::SurfaceKHR surface,
                  SDL_Window *window);
  ~VulkanSwapchain();

  VulkanSwapchain(const VulkanSwapchain &) = delete;
  VulkanSwapchain &operator=(const VulkanSwapchain &) = delete;

  void Recreate();

  vk::SwapchainKHR GetSwapchain() const { return m_Swapchain; }
  vk::Format GetImageFormat() const { return m_ImageFormat; }
  vk::Extent2D GetExtent() const { return m_Extent; }
  const std::vector<vk::Image> &GetImages() const { return m_Images; }
  const std::vector<vk::ImageView> &GetImageViews() const {
    return m_ImageViews;
  }

  static SwapchainSupportDetails
  QuerySwapchainSupport(vk::PhysicalDevice device, vk::SurfaceKHR surface);

private:
  void CreateSwapchain();
  void CreateImageViews();
  void CleanUp();

  vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(
      const std::vector<vk::SurfaceFormatKHR> &availableFormats);
  vk::PresentModeKHR ChooseSwapPresentMode(
      const std::vector<vk::PresentModeKHR> &availablePresentModes);
  vk::Extent2D ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities);

private:
  const std::unique_ptr<VulkanDevice> &m_Device;
  vk::Instance m_Instance;
  vk::SurfaceKHR m_Surface;
  SDL_Window *m_Window;

  vk::SwapchainKHR m_Swapchain = nullptr;
  vk::Format m_ImageFormat;
  vk::Extent2D m_Extent;

  std::vector<vk::Image> m_Images;
  std::vector<vk::ImageView> m_ImageViews;
};

} // namespace mc
