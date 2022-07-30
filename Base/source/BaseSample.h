#pragma once
#include <iostream>
#include <vector>
#include <set>
#include <string>
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vk_mem_alloc.h>
#include "Helpers/VulkanDevice.h"
#include "Helpers/VulkanSwapChain.h"

class BaseSample
{
public:
    // Window
    GLFWwindow*                         base_window = nullptr;
#define                                 WINDOW_WIDTH_SCALE 0.85
#define                                 WINDOW_HEIGHT_SCALE 0.75
    int                                 base_windowWidth = 0;
    int                                 base_windowHeight = 0;
    int                                 base_windowXPos = 0;
    int                                 base_windowYPos = 0;
    bool                                base_framebufferResized = false;

    // Sample should set its requirements

    // App title (setting up by sample)
    std::string                         base_title = "No Title";

    class SampleInstanceRequirements
    {
    public:
        // Used Vulkan API version (setting up by sample)
        uint32_t                        base_instanceApiVersion = VK_API_VERSION_1_1;
        // Enabled(required, setting up by sample) and supported instance extensions
        std::vector<const char*>        base_instanceEnabledExtensionsNames;
    };
    SampleInstanceRequirements base_sampleInstanceRequirements;

    class SampleDeviceRequirements
    {
    public:
        // Enabled(required, setting up by sample) device extensions 
        std::vector<std::string>        base_deviceEnabledExtensionsNames;
        // Enabled(required, setting up by sample) device features
        VkPhysicalDeviceFeatures        base_deviceEnabledFeatures{};
        // Required queue family types(setting up by sample)
        VkQueueFlags                    base_deviceRequiredQueueFamilyTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT;
    };
    SampleDeviceRequirements base_sampleDeviceRequirements;

    class SampleSwapChainRequirements
    {
    public:
        // Preferred surface format, it will be selected if supported, otherwise another will be selected
        VkSurfaceFormatKHR              base_swapChainPrefferedFormat{ VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        // Preferred surface present mode, it will be selected if supported, otherwise another will be selected
        VkPresentModeKHR                base_swapChainPrefferedPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    };
    SampleSwapChainRequirements base_sampleSwapChainRequirements;

    // Instance
    VkInstance                          base_instance = VK_NULL_HANDLE;
    std::vector<VkExtensionProperties>  base_instanceSupportedExtensions;
    
    // Debug
    VkDebugUtilsMessengerEXT            base_debugMessenger = VK_NULL_HANDLE;

    // Current Vulkan device
    VulkanDevice*                       base_vulkanDevice = nullptr;
    // Default graphics queue
    VkQueue                             base_graphicsQueue = VK_NULL_HANDLE;
    // Default compute queue
    VkQueue                             base_computeQueue = VK_NULL_HANDLE;
    // Default transfer queue
    VkQueue                             base_transferQueue = VK_NULL_HANDLE;
    // Default present queue
    VkQueue                             base_presentQueue = VK_NULL_HANDLE;

    // Surface
    VkSurfaceKHR                        base_surface = VK_NULL_HANDLE;

    // SwapChain
    VulkanSwapChain*                    base_vulkanSwapChain = nullptr;

    // General render pass
    VkRenderPass                        base_renderPass = VK_NULL_HANDLE;

    // Framebuffers for swap chain images
    std::vector<VkFramebuffer>          base_swapChainFramebuffers;
public:
    BaseSample();
    virtual ~BaseSample();

    void initVulkan();

    // Always redefined by the sample
    virtual void prepare();

    void initGlfw();

    void createWindow();

    void createInstance();

    std::vector<const char*> getBaseRequiredInstanceExtensionsNames();

    // Returns a list of extensions not supported by the instance
    std::vector<std::string> getInstanceNotSupportedExtensionsNames();

    void setupDebugMessenger();

    void createSurface();

    // Init VulkanDevice
    // Pick physical device
    // Create logical device
    // Get queues
    void prepareDevice();

    // Always redefined by the sample, called when checking the physical device
    // It should check the availability of the required features and mark them as enabled
    // Return FALSE if the device does not meet the requirements
    // Return TRUE if the device is suitable
    virtual bool getEnabledFeatures(VkPhysicalDevice physicalDevice);

    // Create swap chain
    void createSwapChain();

    // Create general render pass
    void createRenderPass();

    // Create renderpass
    void createFramebuffers();

    void finishVulkan();
};

