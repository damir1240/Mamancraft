#include "Mamancraft/Renderer/Vulkan/VulkanSwapchain.hpp"
#include "Mamancraft/Core/Logger.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace mc {

VulkanSwapchain::VulkanSwapchain(const std::unique_ptr<VulkanDevice> &device,
                                 vk::Instance instance, vk::SurfaceKHR surface,
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

  vk::Device device = m_Device->GetLogicalDevice();

  device.waitIdle();

  CleanUp();
  CreateSwapchain();
  CreateImageViews();
}

void VulkanSwapchain::CleanUp() {
  vk::Device device = m_Device->GetLogicalDevice();

  for (auto imageView : m_ImageViews) {
    device.destroyImageView(imageView);
  }
  m_ImageViews.clear();

  if (m_Swapchain) {
    device.destroySwapchainKHR(m_Swapchain);
    m_Swapchain = nullptr;
  }
}

SwapchainSupportDetails
VulkanSwapchain::QuerySwapchainSupport(vk::PhysicalDevice device,
                                       vk::SurfaceKHR surface) {
  SwapchainSupportDetails details;

  details.capabilities = device.getSurfaceCapabilitiesKHR(surface);
  details.formats = device.getSurfaceFormatsKHR(surface);
  details.presentModes = device.getSurfacePresentModesKHR(surface);

  return details;
}

vk::SurfaceFormatKHR VulkanSwapchain::ChooseSwapSurfaceFormat(
    const std::vector<vk::SurfaceFormatKHR> &availableFormats) {
  for (const auto &availableFormat : availableFormats) {
    if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
        availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
      return availableFormat;
    }
  }
  return availableFormats[0];
}

vk::PresentModeKHR VulkanSwapchain::ChooseSwapPresentMode(
    const std::vector<vk::PresentModeKHR> &availablePresentModes) {
  for (const auto &availablePresentMode : availablePresentModes) {
    if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
      return availablePresentMode;
    }
  }
  return vk::PresentModeKHR::eFifo; // Guaranteed to be available
}

vk::Extent2D VulkanSwapchain::ChooseSwapExtent(
    const vk::SurfaceCapabilitiesKHR &capabilities) {
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  } else {
    int width, height;
    SDL_GetWindowSizeInPixels(m_Window, &width, &height);

    vk::Extent2D actualExtent = {static_cast<uint32_t>(width),
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

  vk::SurfaceFormatKHR surfaceFormat =
      ChooseSwapSurfaceFormat(swapchainSupport.formats);
  vk::PresentModeKHR presentMode =
      ChooseSwapPresentMode(swapchainSupport.presentModes);
  vk::Extent2D extent = ChooseSwapExtent(swapchainSupport.capabilities);

  uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;
  if (swapchainSupport.capabilities.maxImageCount > 0 &&
      imageCount > swapchainSupport.capabilities.maxImageCount) {
    imageCount = swapchainSupport.capabilities.maxImageCount;
  }

  vk::SwapchainCreateInfoKHR createInfo;
  createInfo.surface = m_Surface;

  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

  QueueFamilyIndices indices = m_Device->GetQueueFamilyIndices();
  uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(),
                                   indices.presentFamily.value()};

  if (indices.graphicsFamily != indices.presentFamily) {
    createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
  } else {
    createInfo.imageSharingMode = vk::SharingMode::eExclusive;
    createInfo.queueFamilyIndexCount = 0;     // Optional
    createInfo.pQueueFamilyIndices = nullptr; // Optional
  }

  createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
  createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
  createInfo.presentMode = presentMode;
  createInfo.clipped = VK_TRUE;

  createInfo.oldSwapchain = nullptr; // We don't reuse it for now, just rebuild

  try {
    m_Swapchain = m_Device->GetLogicalDevice().createSwapchainKHR(createInfo);
  } catch (const vk::SystemError &e) {
    MC_CRITICAL("Failed to create Vulkan Swapchain! Error: {}", e.what());
    throw std::runtime_error("failed to create swapchain!");
  }

  m_Images = m_Device->GetLogicalDevice().getSwapchainImagesKHR(m_Swapchain);

  m_ImageFormat = surfaceFormat.format;
  m_Extent = extent;

  MC_INFO("Vulkan Swapchain created with extent {0}x{1}", m_Extent.width,
          m_Extent.height);
}

void VulkanSwapchain::CreateImageViews() {
  m_ImageViews.resize(m_Images.size());

  vk::Device device = m_Device->GetLogicalDevice();

  for (size_t i = 0; i < m_Images.size(); i++) {
    vk::ImageViewCreateInfo createInfo;
    createInfo.image = m_Images[i];
    createInfo.viewType = vk::ImageViewType::e2D;
    createInfo.format = m_ImageFormat;

    createInfo.components.r = vk::ComponentSwizzle::eIdentity;
    createInfo.components.g = vk::ComponentSwizzle::eIdentity;
    createInfo.components.b = vk::ComponentSwizzle::eIdentity;
    createInfo.components.a = vk::ComponentSwizzle::eIdentity;

    createInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    try {
      m_ImageViews[i] = device.createImageView(createInfo);
    } catch (const vk::SystemError &e) {
      MC_CRITICAL("Failed to create image views! Error: {}", e.what());
      throw std::runtime_error("failed to create image views!");
    }
  }
}

} // namespace mc
