#pragma once
#include <set>
#include <vector>
#include <string>
#include <optional>
#include <set>
#include <vulkan/vulkan.h>
#include "VulkanDevice.h"

class VulkanDevice
{
public:
    VkPhysicalDevice                              physicalDevice = VK_NULL_HANDLE;
    VkDevice							          logicalDevice = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties                    properties{};
    std::vector<VkExtensionProperties>	          supportedExtensions;
    VkPhysicalDeviceFeatures                      supportedFeatures{};
    VkPhysicalDeviceFeatures                      enabledFeatures{};
    VkPhysicalDeviceMemoryProperties              memoryProperties{};
    std::vector<VkQueueFamilyProperties>          queueFamilyProperties;
    class QueueFamilyIndices
    {
    public:
        std::optional<uint32_t> graphics;
        std::optional<uint32_t> compute;
        std::optional<uint32_t> transfer;
        std::optional<uint32_t> present;
    };
    QueueFamilyIndices queueFamilyIndices;


    VulkanDevice();
    ~VulkanDevice();

    // Collects data about a physical device
    VulkanDevice(VkPhysicalDevice physicalDevice);

    // Check device supports the requested extensions
    bool checkExtensionsSupport(std::vector<std::string> requiredExtensionsNames);

    // Find and get required queue family indices
    // - requiredQueueFamilyTypes
    // If requiredQueueFamilyTypes include VK_QUEUE_COMPUTE_BIT or/and VK_QUEUE_TRANSFER_BIT then
    // it first tries to find a dedicated queue family for them; if that fails,
    // any queue family that supports the requested types is looked for
    // - surface
    // It is also necessary to pass the surface to determine whether any queue family supports presentation to it
    // Can be NULL if you do not need to check the possibility of queues to display on the surface.
    QueueFamilyIndices getQueueFamilyIndices(VkQueueFlags requiredQueueFamilyTypes, VkSurfaceKHR surface);

    VkResult createLogicalDevice(VkPhysicalDeviceFeatures requiredFeatures, std::vector<std::string> requiredExtensionsName, VkQueueFlags requiredQueueFamilyTypes);
};

