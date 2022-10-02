#include "VulkanSwapChain.h"
#include "../ErrorInfo/ErrorInfo.h"
#include <limits> // Necessary for std::numeric_limits
#include <algorithm> // Necessary for std::clamp

VulkanSwapChain::VulkanSwapChain(VulkanDevice* vulkanDevice, VkSurfaceKHR surface, GLFWwindow* window)
{
    this->vulkanDevice = vulkanDevice;
    this->surface = surface;
    this->window = window;

    // Get surface capabilities
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkanDevice->physicalDevice, surface, &surfaceSupportDetails.capabilities);

    // Get surface supported format
    uint32_t supportedFormatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vulkanDevice->physicalDevice, surface, &supportedFormatCount, nullptr);
    surfaceSupportDetails.formats.resize(supportedFormatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(vulkanDevice->physicalDevice, surface, &supportedFormatCount, surfaceSupportDetails.formats.data());

    // Get surface supported present modes
    uint32_t supportedPresentModesCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(vulkanDevice->physicalDevice, surface, &supportedPresentModesCount, nullptr);
    surfaceSupportDetails.presentModes.resize(supportedPresentModesCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(vulkanDevice->physicalDevice, surface, &supportedPresentModesCount, surfaceSupportDetails.presentModes.data());

    // Failed to get data about the surface
    if (surfaceSupportDetails.formats.empty() || surfaceSupportDetails.presentModes.empty()) {
        throw MakeErrorInfo("Failed to get data about the swap chain!");
    }
}

VulkanSwapChain::~VulkanSwapChain()
{
    for (const auto& swapChainImageView : this->imagesViews) {
        if (swapChainImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(this->vulkanDevice->logicalDevice, swapChainImageView, nullptr);
        }
    }
    if (this->swapChain) {
        vkDestroySwapchainKHR(this->vulkanDevice->logicalDevice, this->swapChain, nullptr);
    }
}

void VulkanSwapChain::createSwapChain(VkSurfaceFormatKHR preferredFormat, VkPresentModeKHR preferredPresentMode)
{
    // Set format, present mode and extent
    setPrefferedSwapChainFormat(preferredFormat);
    setPrefferedSwapChainPresentMode(preferredPresentMode);
    setSwapChainExtent();

    // Set swap chain image count
    uint32_t imageCount = surfaceSupportDetails.capabilities.minImageCount;
    // Try to use minImageCount + 1 images to get better performance
    // Spec says: maxImageCount == 0 means that there is no limit on the number of images, though there may be limits related to the total amount of memory used by presentable images.
    if (((imageCount + 1) <= surfaceSupportDetails.capabilities.maxImageCount) ||
        (surfaceSupportDetails.capabilities.maxImageCount == 0)) {
        imageCount += 1;
    }

    //Create swap chain
    VkSwapchainCreateInfoKHR swapChainCreateInfo{};
    swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainCreateInfo.surface = this->surface;
    swapChainCreateInfo.minImageCount = imageCount;
    swapChainCreateInfo.imageFormat = surfaceFormat.format;
    swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapChainCreateInfo.imageExtent = surfaceExtent;
    // For non-stereoscopic-3D applications, this value is 1.
    swapChainCreateInfo.imageArrayLayers = 1;
    // We will render directly into these images
    swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    // We have to decide whether these images will be used within the same queue families
    // If they will be used in queues of different queue families,
    // we must specify these different queue families
    std::set<uint32_t> uniqueQueueFamilyIndeces;
    if (vulkanDevice->queueFamilyIndices.graphics.has_value()) {
        uniqueQueueFamilyIndeces.insert(vulkanDevice->queueFamilyIndices.graphics.value());
    }
    if (vulkanDevice->queueFamilyIndices.compute.has_value()) {
        uniqueQueueFamilyIndeces.insert(vulkanDevice->queueFamilyIndices.compute.value());
    }
    if (vulkanDevice->queueFamilyIndices.transfer.has_value()) {
        uniqueQueueFamilyIndeces.insert(vulkanDevice->queueFamilyIndices.transfer.value());
    }
    if (vulkanDevice->queueFamilyIndices.present.has_value()) {
        uniqueQueueFamilyIndeces.insert(vulkanDevice->queueFamilyIndices.present.value());
    }
    std::vector<uint32_t> uniqueQueueFamilyIndecesVect(uniqueQueueFamilyIndeces.size());
    std::copy(uniqueQueueFamilyIndeces.begin(), uniqueQueueFamilyIndeces.end(), uniqueQueueFamilyIndecesVect.begin());
    // Images will be used within different queue families
    if (uniqueQueueFamilyIndeces.size() == 1) {
        // Images will be used within same queue families
        swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapChainCreateInfo.queueFamilyIndexCount = 0;
        swapChainCreateInfo.pQueueFamilyIndices = nullptr;
    }
    else {
        // Images will be used within different queue families
        swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapChainCreateInfo.queueFamilyIndexCount = uniqueQueueFamilyIndecesVect.size();
        swapChainCreateInfo.pQueueFamilyIndices = uniqueQueueFamilyIndecesVect.data();
    }
    swapChainCreateInfo.preTransform = surfaceSupportDetails.capabilities.currentTransform;
    // Ignore alpha channel
    swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapChainCreateInfo.presentMode = surfacePresentMode;
    // We don't care about the color of pixels that are obscured
    swapChainCreateInfo.clipped = VK_TRUE;
    swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    VkResult result;
    result = vkCreateSwapchainKHR(vulkanDevice->logicalDevice, &swapChainCreateInfo, nullptr, &this->swapChain);
    if (result != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to create a swapchain!");
    }

    // Retrieving the swap chain images
    vkGetSwapchainImagesKHR(vulkanDevice->logicalDevice, swapChain, &imageCount, nullptr);
    images.resize(imageCount);
    vkGetSwapchainImagesKHR(vulkanDevice->logicalDevice, swapChain, &imageCount, images.data());

    // Create image views for swap chain images
    imagesViews.resize(images.size());
    for (uint32_t i = 0; i < imagesViews.size(); i++) {
        VkImageViewCreateInfo imageViewCreateInfo{};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.image = images[i];
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = surfaceFormat.format;
        imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;
        VkResult result;
        result = vkCreateImageView(vulkanDevice->logicalDevice, &imageViewCreateInfo, nullptr, &imagesViews[i]);
        if (result != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to create image view for swap chain image!");
        }
    }
}

bool VulkanSwapChain::setPrefferedSwapChainFormat(VkSurfaceFormatKHR preferredFormat)
{
    for (const auto& supportedSurfaceFormat : surfaceSupportDetails.formats) {
        // preferredFormat is supported
        if (preferredFormat.format == supportedSurfaceFormat.format &&
            preferredFormat.colorSpace == supportedSurfaceFormat.colorSpace) {
            this->surfaceFormat = preferredFormat;
            return true;
        }
    }
    // preferredFormat is not supported
    std::cerr << "Warning: Preffered surface format not supported\n";
    this->surfaceFormat = surfaceSupportDetails.formats[0];
    return false;
}

bool VulkanSwapChain::setPrefferedSwapChainPresentMode(VkPresentModeKHR preferredPresentMode)
{
    for (const auto& supportedSurfacePresentMode : surfaceSupportDetails.presentModes) {
        // preferredPresentMode is supported
        if (preferredPresentMode == supportedSurfacePresentMode) {
            this->surfacePresentMode = preferredPresentMode;
            return true;
        }
    }
    // preferredPresentMode is not supported
    this->surfacePresentMode = VK_PRESENT_MODE_FIFO_KHR; //Only the VK_PRESENT_MODE_FIFO_KHR mode is guaranteed to be supported
    return false;
}

void VulkanSwapChain::setSwapChainExtent()
{
    // Spec says:
    // VkSurfaceCapabilitiesKHR::currentExtent special value (0xFFFFFFFF, 0xFFFFFFFF) indicating
    // that the surface size will be determined by the extent of a swapchain targeting the surface
    if (surfaceSupportDetails.capabilities.currentExtent.width != UINT32_MAX) {
        surfaceExtent = surfaceSupportDetails.capabilities.currentExtent;
        return;
    }
    else {
        int windowWidth = 0, windowHeight = 0;
        glfwGetFramebufferSize(window, &windowWidth, &windowHeight);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(windowWidth),
            static_cast<uint32_t>(windowHeight)
        };

        actualExtent.width = std::clamp(actualExtent.width, surfaceSupportDetails.capabilities.minImageExtent.width, surfaceSupportDetails.capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, surfaceSupportDetails.capabilities.minImageExtent.height, surfaceSupportDetails.capabilities.maxImageExtent.height);
    
        surfaceExtent = actualExtent;
        return;
    }
}
