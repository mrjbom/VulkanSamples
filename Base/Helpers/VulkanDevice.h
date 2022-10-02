#pragma once
#include <set>
#include <vector>
#include <string>
#include <optional>
#include <vulkan/vulkan.h>
#include "VulkanDevice.h"
#include "VulkanTools.h"
#include "vk_mem_alloc.h"

class VulkanDevice
{
public:
    VkPhysicalDevice                              physicalDevice = VK_NULL_HANDLE;
    VkDevice                                      logicalDevice = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties                    properties{};
    std::vector<VkExtensionProperties>            supportedExtensions;
    VkPhysicalDeviceFeatures                      supportedFeatures{};
    VkPhysicalDeviceFeatures                      enabledFeatures{};
    VkPhysicalDeviceMemoryProperties              memoryProperties{};
    std::vector<VkQueueFamilyProperties>          queueFamilyProperties;
    class QueueFamilyIndices
    {
    public:
        std::optional<uint32_t> graphics;
        VkQueueFlags            graphicsFlags = 0; // Graphical queue family flags
        std::optional<uint32_t> compute;
        VkQueueFlags            computeFlags = 0;  // Compute queue family flags
        std::optional<uint32_t> transfer;
        VkQueueFlags            transferFlags = 0; // Transfer queue family flags
        std::optional<uint32_t> present;
    };
    QueueFamilyIndices queueFamilyIndices;

    // Collects and save data about a physical device
    VulkanDevice(VkPhysicalDevice physicalDevice);

    // Destroy logical device
    ~VulkanDevice();

    // Check device supports the requested extensions
    bool checkExtensionsSupport(std::vector<std::string> requiredExtensionsNames);

    // Find and save required queue family indices
    // - requiredQueueFamilyTypes
    // If requiredQueueFamilyTypes include VK_QUEUE_COMPUTE_BIT or/and VK_QUEUE_TRANSFER_BIT then
    // it first tries to find a dedicated queue family for them; if that fails,
    // any queue family that supports the requested types is looked for
    // - surface
    // It is also necessary to pass the surface to determine whether any queue family supports presentation to it
    // Can be NULL if you do not need to check the possibility of queues to display on the surface.
    void findQueueFamilyIndices(VkQueueFlags requiredQueueFamilyTypes, VkSurfaceKHR surface);

    // Create logical device
    void createLogicalDevice(VkPhysicalDeviceFeatures requiredFeatures, std::vector<std::string> requiredExtensionsNames);

    // Allocates the command buffer from the command pool and starts recording commands to it
    VkCommandBuffer beginSingleTimeCommands(VkCommandPool commandPool);

    // Completes recording to command buffer, and submit it to the queue for execution
    // Waits until the command buffer has been executed and free it
    // The command pool must be created with the VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT flag
    void endSingleTimeCommands(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool commandPool);
};
