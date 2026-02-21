#include "Mamancraft/Renderer/VulkanContext.hpp"
#include "Mamancraft/Core/Logger.hpp"

#include <SDL3/SDL_vulkan.h>
#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>

using namespace vk;

namespace mc {

static VKAPI_ATTR VkBool32 VKAPI_CALL
DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageType,
              const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
              void *pUserData) {

  if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    MC_ERROR("Vulkan Validation: {0}", pCallbackData->pMessage);
  } else if (messageSeverity >=
             VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    MC_WARN("Vulkan Validation: {0}", pCallbackData->pMessage);
  } else {
    MC_TRACE("Vulkan Validation: {0}", pCallbackData->pMessage);
  }

  return VK_FALSE;
}

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pDebugMessenger) {
  auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkCreateDebugUtilsMessengerEXT");
  if (func != nullptr) {
    return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                   VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks *pAllocator) {
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != nullptr) {
    func(instance, debugMessenger, pAllocator);
  }
}

VulkanContext::VulkanContext(SDL_Window *window) : m_Window(window) { Init(); }

VulkanContext::~VulkanContext() { Shutdown(); }

void VulkanContext::Init() {
  MC_INFO("Initializing Vulkan Context");
  CreateInstance();
  SetupDebugMessenger();
  CreateSurface();
  PickPhysicalDevice();
  CreateLogicalDevice();
  CreateSwapchain();
  CreateAllocator();
  CreateCommandPool();

  vk::FenceCreateInfo fenceCreateInfo;
  fenceCreateInfo.flags = vk::FenceCreateFlagBits::eSignaled;
  m_ImmediateFence = m_Device->GetLogicalDevice().createFence(fenceCreateInfo);
}

void VulkanContext::Shutdown() {
  MC_INFO("VulkanContext::Shutdown() - Starting Vulkan cleanup");

  // Best Practice: Wait for device to be idle before destroying resources
  if (m_Device) {
    MC_DEBUG("VulkanContext: Waiting for device to become idle...");
    m_Device->GetLogicalDevice().waitIdle();
    MC_DEBUG("VulkanContext: Device is now idle");
  }

  // Destroy resources in reverse order of creation
  // 1. Immediate fence (created last in Init)
  if (m_ImmediateFence && m_Device) {
    MC_DEBUG("VulkanContext: Destroying immediate fence...");
    m_Device->GetLogicalDevice().destroyFence(m_ImmediateFence);
    m_ImmediateFence = nullptr;
  }

  // 2. Command pool (depends on device)
  MC_DEBUG("VulkanContext: Destroying command pool...");
  m_CommandPool.reset();

  // 3. Allocator (VMA - CRITICAL: must be destroyed AFTER all buffers are freed)
  MC_DEBUG("VulkanContext: Destroying VMA allocator...");
  m_Allocator.reset();
  MC_DEBUG("VulkanContext: VMA allocator destroyed");

  // 4. Swapchain (depends on device and surface)
  MC_DEBUG("VulkanContext: Destroying swapchain...");
  m_Swapchain.reset();

  // 5. Logical device
  MC_DEBUG("VulkanContext: Destroying logical device...");
  m_Device.reset();

  // 6. Surface (depends on instance)
  if (m_Surface) {
    MC_DEBUG("VulkanContext: Destroying surface...");
    m_Instance.destroySurfaceKHR(m_Surface);
    m_Surface = nullptr;
  }

  // 7. Debug messenger (depends on instance)
  if (m_EnableValidationLayers && m_DebugMessenger) {
    MC_DEBUG("VulkanContext: Destroying debug messenger...");
    DestroyDebugUtilsMessengerEXT(
        static_cast<VkInstance>(m_Instance),
        static_cast<VkDebugUtilsMessengerEXT>(m_DebugMessenger), nullptr);
    m_DebugMessenger = nullptr;
  }

  // 8. Instance (destroyed last)
  if (m_Instance) {
    MC_DEBUG("VulkanContext: Destroying Vulkan instance...");
    m_Instance.destroy();
    m_Instance = nullptr;
  }
  
  MC_INFO("VulkanContext::Shutdown() - Vulkan cleanup completed");
}

void VulkanContext::CreateInstance() {
  if (m_EnableValidationLayers && !CheckValidationLayerSupport()) {
    MC_CRITICAL("Validation layers requested, but not available!");
    throw std::runtime_error("validation layers requested, but not available!");
  }

  ApplicationInfo appInfo("Mamancraft", VK_MAKE_VERSION(1, 0, 0),
                          "Mamancraft Engine", VK_MAKE_VERSION(1, 0, 0),
                          VK_API_VERSION_1_4);

  InstanceCreateInfo createInfo;
  createInfo.pApplicationInfo = &appInfo;

  auto extensions = GetRequiredExtensions();
  createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  createInfo.ppEnabledExtensionNames = extensions.data();

  DebugUtilsMessengerCreateInfoEXT debugCreateInfo;
  if (m_EnableValidationLayers) {
    createInfo.enabledLayerCount =
        static_cast<uint32_t>(m_ValidationLayers.size());
    createInfo.ppEnabledLayerNames = m_ValidationLayers.data();

    debugCreateInfo.messageSeverity =
        DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
        DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
        DebugUtilsMessageSeverityFlagBitsEXT::eError;
    debugCreateInfo.messageType =
        DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
        DebugUtilsMessageTypeFlagBitsEXT::eValidation |
        DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
    debugCreateInfo.pfnUserCallback =
        reinterpret_cast<vk::PFN_DebugUtilsMessengerCallbackEXT>(DebugCallback);
    createInfo.pNext = &debugCreateInfo;
  } else {
    createInfo.enabledLayerCount = 0;
    createInfo.pNext = nullptr;
  }

  try {
    m_Instance = createInstance(createInfo);
  } catch (const SystemError &e) {
    MC_CRITICAL("Failed to create Vulkan instance! Error: {}", e.what());
    throw std::runtime_error("failed to create instance!");
  }
  MC_INFO("Vulkan Instance created successfully.");
}

bool VulkanContext::CheckValidationLayerSupport() {
  std::vector<LayerProperties> availableLayers =
      enumerateInstanceLayerProperties();

  for (const char *layerName : m_ValidationLayers) {
    bool layerFound = false;
    for (const auto &layerProperties : availableLayers) {
      if (strcmp(layerName, layerProperties.layerName) == 0) {
        layerFound = true;
        break;
      }
    }
    if (!layerFound) {
      return false;
    }
  }
  return true;
}

std::vector<const char *> VulkanContext::GetRequiredExtensions() {
  uint32_t sdlExtensionCount = 0;
  char const *const *sdlExtensions =
      SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);

  if (!sdlExtensions && sdlExtensionCount == 0) {
    MC_ERROR(
        "SDL_Vulkan_GetInstanceExtensions failed or returned 0 extensions: {0}",
        SDL_GetError());
  }

  std::vector<const char *> extensions(sdlExtensions,
                                       sdlExtensions + sdlExtensionCount);

  if (m_EnableValidationLayers) {
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  return extensions;
}

void VulkanContext::SetupDebugMessenger() {
  if (!m_EnableValidationLayers)
    return;

  DebugUtilsMessengerCreateInfoEXT createInfo;
  createInfo.messageSeverity = DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                               DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                               DebugUtilsMessageSeverityFlagBitsEXT::eError;
  createInfo.messageType = DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                           DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                           DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
  createInfo.pfnUserCallback =
      reinterpret_cast<vk::PFN_DebugUtilsMessengerCallbackEXT>(DebugCallback);

  // Since CreateDebugUtilsMessengerEXT is an extension function,
  // we use standard C approach for binding with Vulkan-HPP dispatch loader if
  // we aren't using Vulkan-HPP dispatch
  VkDebugUtilsMessengerCreateInfoEXT cInfo =
      static_cast<VkDebugUtilsMessengerCreateInfoEXT>(createInfo);
  VkDebugUtilsMessengerEXT cDebugMessenger;
  if (CreateDebugUtilsMessengerEXT(static_cast<VkInstance>(m_Instance), &cInfo,
                                   nullptr, &cDebugMessenger) != VK_SUCCESS) {
    MC_CRITICAL("Failed to set up debug messenger!");
    throw std::runtime_error("failed to set up debug messenger!");
  }
  m_DebugMessenger = cDebugMessenger;
  MC_INFO("Vulkan Debug Messenger set up successfully.");
}

void VulkanContext::CreateSurface() {
  VkSurfaceKHR cSurface;
  if (!SDL_Vulkan_CreateSurface(m_Window, static_cast<VkInstance>(m_Instance),
                                nullptr, &cSurface)) {
    MC_CRITICAL("Failed to create Vulkan surface: {0}", SDL_GetError());
    throw std::runtime_error("failed to create vulkan surface!");
  }
  m_Surface = cSurface;
  MC_INFO("Vulkan Surface created successfully.");
}

QueueFamilyIndices VulkanContext::FindQueueFamilies(vk::PhysicalDevice device) {
  QueueFamilyIndices indices;

  std::vector<QueueFamilyProperties> queueFamilies =
      device.getQueueFamilyProperties();

  int i = 0;
  for (const auto &queueFamily : queueFamilies) {
    if (queueFamily.queueFlags & QueueFlagBits::eGraphics) {
      indices.graphicsFamily = i;
    }

    if (queueFamily.queueFlags & QueueFlagBits::eCompute) {
      indices.computeFamily = i;
    }

    if (device.getSurfaceSupportKHR(i, m_Surface)) {
      indices.presentFamily = i;
    }

    if (indices.isComplete()) {
      break;
    }

    i++;
  }

  return indices;
}

void VulkanContext::PickPhysicalDevice() {
  std::vector<vk::PhysicalDevice> devices =
      m_Instance.enumeratePhysicalDevices();

  if (devices.empty()) {
    MC_CRITICAL("Failed to find GPUs with Vulkan support!");
    throw std::runtime_error("failed to find GPUs with Vulkan support!");
  }

  for (const auto &device : devices) {
    vk::PhysicalDeviceProperties deviceProperties = device.getProperties();
    MC_INFO("Checking Vulkan Physical Device: {0}",
            deviceProperties.deviceName.data());

    QueueFamilyIndices indices = FindQueueFamilies(device);
    bool extensionsSupported = CheckDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
      SwapchainSupportDetails swapChainSupport =
          VulkanSwapchain::QuerySwapchainSupport(
              static_cast<VkPhysicalDevice>(device),
              static_cast<VkSurfaceKHR>(m_Surface));
      swapChainAdequate = !swapChainSupport.formats.empty() &&
                          !swapChainSupport.presentModes.empty();
    }

    if (indices.isComplete() && extensionsSupported && swapChainAdequate) {
      m_PhysicalDevice = device;
      break;
    }
  }

  if (!m_PhysicalDevice) {
    MC_CRITICAL("Failed to find a suitable GPU!");
    throw std::runtime_error("failed to find a suitable GPU!");
  }

  vk::PhysicalDeviceProperties props = m_PhysicalDevice.getProperties();
  MC_INFO("Selected Vulkan Physical Device: {0}", props.deviceName.data());
}

void VulkanContext::CreateLogicalDevice() {
  QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);

  std::vector<DeviceQueueCreateInfo> queueCreateInfos;
  std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(),
                                            indices.presentFamily.value(),
                                            indices.computeFamily.value()};

  float queuePriority = 1.0f;
  for (uint32_t queueFamily : uniqueQueueFamilies) {
    DeviceQueueCreateInfo queueCreateInfo;
    queueCreateInfo.queueFamilyIndex = queueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(queueCreateInfo);
  }

  PhysicalDeviceFeatures deviceFeatures;

  // Modern dynamic rendering features require Vulkan 1.3 or KHR extensions
  // We will enable dynamic rendering here
  PhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures;
  dynamicRenderingFeatures.dynamicRendering = true;

  DeviceCreateInfo createInfo;
  createInfo.queueCreateInfoCount =
      static_cast<uint32_t>(queueCreateInfos.size());
  createInfo.pQueueCreateInfos = queueCreateInfos.data();

  createInfo.pEnabledFeatures = &deviceFeatures;
  createInfo.pNext = &dynamicRenderingFeatures;

  // We are on Vulkan 1.4, so Dynamic Rendering is core, but we can also request
  // the Vulkan 1.3 feature structure
  PhysicalDeviceVulkan13Features features13;
  features13.dynamicRendering = true;
  createInfo.pNext = &features13;

  createInfo.enabledExtensionCount =
      static_cast<uint32_t>(m_DeviceExtensions.size());
  createInfo.ppEnabledExtensionNames = m_DeviceExtensions.data();

  if (m_EnableValidationLayers) {
    createInfo.enabledLayerCount =
        static_cast<uint32_t>(m_ValidationLayers.size());
    createInfo.ppEnabledLayerNames = m_ValidationLayers.data();
  } else {
    createInfo.enabledLayerCount = 0;
  }

  vk::Device logicalDevice;
  try {
    logicalDevice = m_PhysicalDevice.createDevice(createInfo);
  } catch (const SystemError &e) {
    MC_CRITICAL("Failed to create logical device! Error: {}", e.what());
    throw std::runtime_error("failed to create logical device!");
  }

  m_Device = std::make_unique<VulkanDevice>(
      static_cast<VkPhysicalDevice>(m_PhysicalDevice),
      static_cast<VkDevice>(logicalDevice), indices);

  MC_INFO("Vulkan Logical Device and Queues created successfully.");
}

bool VulkanContext::CheckDeviceExtensionSupport(vk::PhysicalDevice device) {
  std::vector<ExtensionProperties> availableExtensions =
      device.enumerateDeviceExtensionProperties();

  std::set<std::string> requiredExtensions(m_DeviceExtensions.begin(),
                                           m_DeviceExtensions.end());

  for (const auto &extension : availableExtensions) {
    requiredExtensions.erase(extension.extensionName.data());
  }

  return requiredExtensions.empty();
}

void VulkanContext::CreateSwapchain() {
  m_Swapchain = std::make_unique<VulkanSwapchain>(
      m_Device, static_cast<VkInstance>(m_Instance),
      static_cast<VkSurfaceKHR>(m_Surface), m_Window);
}

void VulkanContext::CreateAllocator() {
  m_Allocator = std::make_unique<VulkanAllocator>(
      static_cast<VkInstance>(m_Instance), m_Device);
}

void VulkanContext::CreateCommandPool() {
  uint32_t graphicsQueueFamily =
      m_Device->GetQueueFamilyIndices().graphicsFamily.value();
  m_CommandPool = std::make_unique<VulkanCommandPool>(
      m_Device, graphicsQueueFamily,
      vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
}

void VulkanContext::ImmediateSubmit(
    std::function<void(vk::CommandBuffer)> &&func) {
  std::unique_ptr<VulkanCommandBuffer> commandBuffer =
      m_CommandPool->AllocateCommandBuffer(true);

  commandBuffer->Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
  func(commandBuffer->GetCommandBuffer());
  commandBuffer->End();

  vk::SubmitInfo submitInfo;
  submitInfo.commandBufferCount = 1;
  vk::CommandBuffer cmdBuf = commandBuffer->GetCommandBuffer();
  submitInfo.pCommandBuffers = &cmdBuf;

  (void)m_Device->GetLogicalDevice().resetFences(1, &m_ImmediateFence);
  m_Device->GetGraphicsQueue().submit(submitInfo, m_ImmediateFence);

  if (m_Device->GetLogicalDevice().waitForFences(
          m_ImmediateFence, VK_TRUE, 10000000000ULL) != vk::Result::eSuccess) {
    MC_ERROR("ImmediateSubmit: wait for fence failed or timed out!");
  }
}

} // namespace mc
