#include "Mamancraft/Renderer/Vulkan/VulkanSwapchain.hpp"
#include "Mamancraft/Core/Logger.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace mc {

VulkanSwapchain::VulkanSwapchain(const std::unique_ptr<VulkanDevice> &device,
                                 VkInstance instance, VkSurfaceKHR surface,
                                 SDL_Window *window)
    : m_Device(device), m_Instance(instance), m_Surface(surface),
      m_Window(window) {
  CreateSwapchain();
  CreateImageViews();
}

VulkanSwapchain::~VulkanSwapchain() { CleanUp(); }

void VulkanSwapchain::Recreate() {
  int width = 0, height = 0;
  SDL_GetWindowSizeInPixels(m_Window, &width, &height);

  // Handle minimization
  while (width == 0 || height == 0) {
    SDL_GetWindowSizeInPixels(m_Window, &width, &height);
    SDL_WaitEvent(nullptr);
  }

  vkDeviceWaitIdle(m_Device->GetLogicalDevice());

  CleanUp();
  CreateSwapchain();
  CreateImageViews();
}

void VulkanSwapchain::CleanUp() {
  VkDevice device = m_Device->GetLogicalDevice();

  for (auto imageView : m_ImageViews) {
    vkDestroyImageView(device, imageView, nullptr);
  }
  m_ImageViews.clear();

  if (m_Swapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device, m_Swapchain, nullptr);
    m_Swapchain = VK_NULL_HANDLE;
  }
}

SwapchainSupportDetails
VulkanSwapchain::QuerySwapchainSupport(VkPhysicalDevice device,
                                       VkSurfaceKHR surface) {
  SwapchainSupportDetails details;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface,
                                            &details.capabilities);

  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
  if (formatCount != 0) {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount,
                                         details.formats.data());
  }

  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount,
                                            nullptr);
  if (presentModeCount != 0) {
    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device, surface, &presentModeCount, details.presentModes.data());
  }

  return details;
}

VkSurfaceFormatKHR VulkanSwapchain::ChooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR> &availableFormats) {
  for (const auto &availableFormat : availableFormats) {
    if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
        availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return availableFormat;
    }
  }
  return availableFormats[0];
}

VkPresentModeKHR VulkanSwapchain::ChooseSwapPresentMode(
    const std::vector<VkPresentModeKHR> &availablePresentModes) {
  for (const auto &availablePresentMode : availablePresentModes) {
    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
      return availablePresentMode;
    }
  }
  return VK_PRESENT_MODE_FIFO_KHR; // Guaranteed to be available
}

VkExtent2D VulkanSwapchain::ChooseSwapExtent(
    const VkSurfaceCapabilitiesKHR &capabilities) {
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  } else {
    int width, height;
    SDL_GetWindowSizeInPixels(m_Window, &width, &height);

    VkExtent2D actualExtent = {static_cast<uint32_t>(width),
                               static_cast<uint32_t>(height)};

    actualExtent.width =
        std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                   capabilities.maxImageExtent.width);
    actualExtent.height =
        std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                   capabilities.maxImageExtent.height);

    return actualExtent;
  }
}

void VulkanSwapchain::CreateSwapchain() {
  SwapchainSupportDetails swapchainSupport =
      QuerySwapchainSupport(m_Device->GetPhysicalDevice(), m_Surface);

  VkSurfaceFormatKHR surfaceFormat =
      ChooseSwapSurfaceFormat(swapchainSupport.formats);
  VkPresentModeKHR presentMode =
      ChooseSwapPresentMode(swapchainSupport.presentModes);
  VkExtent2D extent = ChooseSwapExtent(swapchainSupport.capabilities);

  uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;
  if (swapchainSupport.capabilities.maxImageCount > 0 &&
      imageCount > swapchainSupport.capabilities.maxImageCount) {
    imageCount = swapchainSupport.capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = m_Surface;

  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  QueueFamilyIndices indices = m_Device->GetQueueFamilyIndices();
  uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(),
                                   indices.presentFamily.value()};

  if (indices.graphicsFamily != indices.presentFamily) {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
  } else {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;     // Optional
    createInfo.pQueueFamilyIndices = nullptr; // Optional
  }

  createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode = presentMode;
  createInfo.clipped = VK_TRUE;

  createInfo.oldSwapchain =
      VK_NULL_HANDLE; // We don't reuse it for now, just rebuild

  if (vkCreateSwapchainKHR(m_Device->GetLogicalDevice(), &createInfo, nullptr,
                           &m_Swapchain) != VK_SUCCESS) {
    MC_CRITICAL("Failed to create Vulkan Swapchain!");
    throw std::runtime_error("failed to create swapchain!");
  }
  vkGetSwapchainImagesKHR(m_Device->GetLogicalDevice(), m_Swapchain,
                          &imageCount, nullptr);
  m_Images.resize(imageCount);
  vkGetSwapchainImagesKHR(m_Device->GetLogicalDevice(), m_Swapchain,
                          &imageCount, m_Images.data());

  m_ImageFormat = surfaceFormat.format;
  m_Extent = extent;

  MC_INFO("Vulkan Swapchain created with extent {0}x{1}", m_Extent.width,
          m_Extent.height);
}

void VulkanSwapchain::CreateImageViews() {
  m_ImageViews.resize(m_Images.size());

  VkDevice device = m_Device->GetLogicalDevice();

  for (size_t i = 0; i < m_Images.size(); i++) {
    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = m_Images[i];
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = m_ImageFormat;

    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &createInfo, nullptr, &m_ImageViews[i]) !=
        VK_SUCCESS) {
      MC_CRITICAL("Failed to create image views!");
      throw std::runtime_error("failed to create image views!");
    }
  }
}

} // namespace mc
