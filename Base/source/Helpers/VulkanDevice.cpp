#include "VulkanDevice.h"
#include "../ErrorInfo/ErrorInfo.h"
#include <algorithm>

VulkanDevice::VulkanDevice()
{
}

VulkanDevice::~VulkanDevice()
{
	if (this->logicalDevice) {
		vkDestroyDevice(this->logicalDevice, nullptr);
	}
}

VulkanDevice::VulkanDevice(VkPhysicalDevice physicalDevice)
{
    this->physicalDevice = physicalDevice;

    // Get device properties
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);

    // Get device supported extensions
    uint32_t supportedExtensionsCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &supportedExtensionsCount, nullptr);
    supportedExtensions.resize(supportedExtensionsCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &supportedExtensionsCount, supportedExtensions.data());
    
	// Get device supported features
    vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);
    
	// Get device memory properties
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
    
	// Get device queue families
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    queueFamilyProperties.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());
}

bool VulkanDevice::checkExtensionsSupport(std::vector<std::string> requiredExtensionsNames)
{
    std::set<std::string> notSupportedExtensionsNames;
    for (auto& extensionName : requiredExtensionsNames) {
        notSupportedExtensionsNames.insert(extensionName);
    }
    for (auto& extension : supportedExtensions) {
        notSupportedExtensionsNames.erase(extension.extensionName);
    }
    return notSupportedExtensionsNames.empty();
}

void VulkanDevice::getQueueFamilyIndices(VkQueueFlags requiredQueueFamilyTypes, VkSurfaceKHR surface)
{
    VulkanDevice::QueueFamilyIndices indices;

    // Search for any queue family that supports graphical operations
    if (requiredQueueFamilyTypes & VK_QUEUE_GRAPHICS_BIT) {
        for (uint32_t i = 0; i < queueFamilyProperties.size(); ++i) {
            if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphics = i;
                indices.graphicsFlags = queueFamilyProperties[i].queueFlags;
                break;
            }
        }
        // We have not found a required queue family
        if (!indices.graphics.has_value()) {
            throw MakeErrorInfo("Could not find a queue family that supports graphical operations!");
        }
    }

    // Search for dedicated or any queue family that supports compute operations
    if (requiredQueueFamilyTypes & VK_QUEUE_COMPUTE_BIT) {
        // Dedicated queue family - supports VK_QUEUE_COMPUTE_BIT but no VK_QUEUE_GRAPHICS_BIT
        
        // The index of the queue family that supports VK_QUEUE_COMPUTE_BIT but is not necessarily dedicated
        // It is used if was not possible to find a dedicated(which supports only VK_QUEUE_COMPUTE_BIT, but not VK_QUEUE_GRAPHICS_BIT).
        std::optional<uint32_t> first_any;

        for (uint32_t i = 0; i < queueFamilyProperties.size(); ++i) {
            // The first "any"(not dedicated) queue family has been found
            if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && (!first_any.has_value())) {
                first_any = i;
            }
            // The first "dedicated" queue family has been found
            // Support compute operations, but not graphics
            if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && !(queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                // Dedicated queue family found successfully
                indices.compute = i;
                indices.computeFlags = queueFamilyProperties[i].queueFlags;
                break;
            }
        }
        // We not found dedicated queue family for compute operations
        if (!indices.compute.has_value()) {
            // We not found any queue family that support compute operations
            if (!first_any.has_value()) {
                throw MakeErrorInfo("Could not find a queue family that supports compute operations!");
            }
            else {
                // We use any queue family that support compute operations
                indices.compute = first_any;
                indices.computeFlags = queueFamilyProperties[first_any.value()].queueFlags;
            }
        }
    }

    // Search for dedicated or any queue family that supports transfer operations
    if (requiredQueueFamilyTypes & VK_QUEUE_TRANSFER_BIT) {
        // Dedicated queue family - supports VK_QUEUE_TRANSFER_BIT but no VK_QUEUE_GRAPHICS_BIT

        // The index of the queue family that supports VK_QUEUE_TRANSFER_BIT but is not necessarily dedicated
        // It is used if was not possible to find a dedicated(which supports only VK_QUEUE_TRANSFER_BIT, but not VK_QUEUE_GRAPHICS_BIT).
        std::optional<uint32_t> first_any;

        for (uint32_t i = 0; i < queueFamilyProperties.size(); ++i) {
            // The first "any"(not dedicated) queue family has been found
            if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) && (!first_any.has_value())) {
                first_any = i;
            }
            // The first "dedicated" queue family has been found
            // Support compute operations, but not graphics
            if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) && !(queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                // Dedicated queue family found successfully
                indices.transfer = i;
                indices.transferFlags = queueFamilyProperties[i].queueFlags;
                break;
            }
        }
        // We not found dedicated queue family for transfer operations
        if (!indices.transfer.has_value()) {
            // We not found any queue family that support compute operations
            if (!first_any.has_value()) {
                // Spec says:
                // All commands that are allowed on a queue that supports transfer operations
                // are also allowed on a queue that supports either graphics or compute operations.
                // Thus, if the capabilities of a queue family include VK_QUEUE_GRAPHICS_BIT or VK_QUEUE_COMPUTE_BIT,
                // then reporting the VK_QUEUE_TRANSFER_BIT capability separately for that queue family is optional.
                if (indices.graphics.has_value()) {
                    indices.transfer = indices.graphics;
                    indices.transferFlags = indices.graphicsFlags;
                } 
                else if (indices.compute.has_value()) {
                    indices.transfer = indices.compute;
                    indices.transferFlags = indices.computeFlags;
                }
                else {
                    throw MakeErrorInfo("Could not find a queue family that supports transfer operations!");
                }
            }
            else {
                // We use any queue family that support compute operations
                indices.transfer = first_any;
                indices.transferFlags = queueFamilyProperties[first_any.value()].queueFlags;
            }
        }
    }

    // Find queue family that support present
    if (surface && !indices.present.has_value()) {
        VkBool32 presentSupported = VK_FALSE;
        for (uint32_t i = 0; i < queueFamilyProperties.size(); ++i) {
            vkGetPhysicalDeviceSurfaceSupportKHR(this->physicalDevice, i, surface, &presentSupported);
            if (presentSupported) {
                indices.present = i;
                break;
            }
        }
        // We not found queue family that support present
        if(!indices.present.has_value()) {
            throw MakeErrorInfo("Could not find a queue family that supports present!");
        }
    }
    this->queueFamilyIndices = indices;
}

void VulkanDevice::createLogicalDevice(VkPhysicalDeviceFeatures requiredFeatures, std::vector<std::string> requiredExtensionsNames)
{
    // We already have a logical device and cannot create a second one
    if (this->logicalDevice) {
        throw MakeErrorInfo("Creating a logical device with already existing logical device!")
    }

    // Create queue create infos
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

    // Spec says
    // The queueFamilyIndex member of each element of pQueueCreateInfos must be unique within pQueueCreateInfos,
    // except that two members can share the same queueFamilyIndex 
    // if one describes protected - capable queues and one describes queues that are not protected - capable
    
    std::vector<uint32_t> uniqueQueueFamilyIndeces;
    if (queueFamilyIndices.graphics.has_value()) {
        uniqueQueueFamilyIndeces.push_back(queueFamilyIndices.graphics.value());
    }
    if (queueFamilyIndices.compute.has_value()) {
        uniqueQueueFamilyIndeces.push_back(queueFamilyIndices.compute.value());
    }
    if (queueFamilyIndices.transfer.has_value()) {
        uniqueQueueFamilyIndeces.push_back(queueFamilyIndices.transfer.value());
    }
    if (queueFamilyIndices.present.has_value()) {
        uniqueQueueFamilyIndeces.push_back(queueFamilyIndices.present.value());
    }
    // remove consecutive (adjacent) duplicates
    auto lastElem = std::unique(uniqueQueueFamilyIndeces.begin(), uniqueQueueFamilyIndeces.end());
    uniqueQueueFamilyIndeces.erase(lastElem, uniqueQueueFamilyIndeces.end());
    // sort followed by unique, to remove all duplicates
    std::sort(uniqueQueueFamilyIndeces.begin(), uniqueQueueFamilyIndeces.end());
    // remove duplicates
    lastElem = std::unique(uniqueQueueFamilyIndeces.begin(), uniqueQueueFamilyIndeces.end());
    uniqueQueueFamilyIndeces.erase(lastElem, uniqueQueueFamilyIndeces.end());

    for (auto& uniqueQueueFamilyIndex : uniqueQueueFamilyIndeces) {
        queueCreateInfos.push_back({});
        queueCreateInfos.back().sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfos.back().queueFamilyIndex = uniqueQueueFamilyIndex;
        queueCreateInfos.back().queueCount = 1;
        float queuePriority = 1.0f;
        queueCreateInfos.back().pQueuePriorities = &queuePriority;
    }

    // Create logical device
    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = queueCreateInfos.size();
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    std::vector<const char*> requiredExtensionsNames_const_char_ptr;
    for (auto& requiredExtensionsName : requiredExtensionsNames) {
        requiredExtensionsNames_const_char_ptr.push_back(requiredExtensionsName.c_str());
    }
    deviceCreateInfo.enabledExtensionCount = requiredExtensionsNames_const_char_ptr.size();
    deviceCreateInfo.ppEnabledExtensionNames = requiredExtensionsNames_const_char_ptr.data();
    deviceCreateInfo.pEnabledFeatures = &requiredFeatures;

    VkResult result;
    result = vkCreateDevice(this->physicalDevice, &deviceCreateInfo, nullptr, &this->logicalDevice);
    if (result != VK_SUCCESS && result != VK_ERROR_FEATURE_NOT_PRESENT) {
        throw MakeErrorInfo("Failed to create a logical device!");
    }
    else if (result == VK_ERROR_FEATURE_NOT_PRESENT) {
        throw MakeErrorInfo("The physical device does not support the required feature! It looks like the sample incorrectly checks and set the required features");
    }
}