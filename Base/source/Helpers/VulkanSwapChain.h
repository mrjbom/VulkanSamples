#pragma once
#include <set>
#include <vector>
#include <string>
#include <optional>
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "VulkanDevice.h"

class VulkanSwapChain
{
public:
    GLFWwindow*                 window = nullptr;
    VulkanDevice*               vulkanDevice = nullptr;
    VkSurfaceKHR                surface = VK_NULL_HANDLE;
    VkSwapchainKHR              swapChain = VK_NULL_HANDLE;
    std::vector<VkImage>        swapChainImages;
    std::vector<VkImageView>    swapChainImagesViews;
    class SurfaceSupportDetails
    {
    public:
        VkSurfaceCapabilitiesKHR        surfaceCapabilities{};
        std::vector<VkSurfaceFormatKHR> supportedSurfaceFormats;
        std::vector<VkPresentModeKHR>   supportedSurfacePresentModes;
    };
    SurfaceSupportDetails       surfaceSupportDetails;

    VkSurfaceFormatKHR          swapChainFormat{};
    VkPresentModeKHR            swapChainPresentMode{};
    VkExtent2D                  swapChainExtent{};
    
    // Collects and save data about swapchain for VulkanDevice
    // Choose optimal swap chain parameters (format, present mode and extent)
    VulkanSwapChain(VulkanDevice* vulkanDevice, VkSurfaceKHR surface, GLFWwindow* window);
    ~VulkanSwapChain();
    
    // Choose format, present mode, extent
    // Checks whether preferred format and present mode is supported by surface
    // Creates a swapchain
    // Retrieving the swap chain images
    void createSwapChain(VkSurfaceFormatKHR preferredFormat, VkPresentModeKHR preferredPresentMode);

    // Checks whether preferredFormat is supported by surface,
    // if it is available, it will be selected and the function will return true,
    // otherwise another supported format will be selected and the function will return false(this does not mean that further work is impossible)
    bool setPrefferedSwapChainFormat(VkSurfaceFormatKHR preferredFormat);

    // Checks whether preferredPresentMode is supported by surface,
    // if it is available, it will be selected and the function will return true,
    // otherwise another supported present mode will be selected and the function will return false(this does not mean that further work is impossible)
    bool setPrefferedSwapChainPresentMode(VkPresentModeKHR preferredPresentMode);

    // Set swap chain image extent using surface capabilities
    void setSwapChainExtent();
};
