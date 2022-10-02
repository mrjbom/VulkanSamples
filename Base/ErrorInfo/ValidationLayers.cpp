#include "ValidationLayers.h"
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <iostream>

const std::vector<const char*> ValidationLayers::validationLayers = {
        "VK_LAYER_KHRONOS_validation"
};

bool ValidationLayers::checkSupport()
{
    uint32_t layersCount = 0;
    //Get count
    vkEnumerateInstanceLayerProperties(&layersCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layersCount);
    //Get layers
    vkEnumerateInstanceLayerProperties(&layersCount, availableLayers.data());
    
    //Check layers support
    for (const char* layerName : validationLayers) {
        bool layerFound = false;

        for (const VkLayerProperties& avaiableLayerProperties : availableLayers) {
            if (strcmp(layerName, avaiableLayerProperties.layerName) == 0) {
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

VkResult ValidationLayers::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void ValidationLayers::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL ValidationLayers::debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {
    /*
    messageSeverity values
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: Diagnostic message
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT : Informational message like the creation of a resource
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT : Message about behavior that is not necessarily an error, but very likely a bug in your application
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT : Message about behavior that is invalidand may cause crashes
    */

    /*
    messageType values
    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT: Some event has happened that is unrelated to the specification or performance
    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT : Something has happened that violates the specification or indicates a possible mistake
    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT : Potential non - optimal use of Vulkan
    */

    std::string debugInfoStr = "";
    std::string messageSeverityStr = "";
    std::string messageTypeStr = "";
    std::string message = "";

    switch (messageSeverity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        messageSeverityStr = "Diagnostic";
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        messageSeverityStr = "Informational";
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        messageSeverityStr = "WARNING";
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        messageSeverityStr = "ERROR";
        break;
    default:
        messageSeverityStr = "Not recognized";
    }

    switch (messageType)
    {
    case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
        messageTypeStr = "Not specification/performance";
        break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
        messageTypeStr = "Specification violation or possible error";
        break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
        messageTypeStr = "Potential non-optimal use of Vulkan";
        break;
    default:
        messageTypeStr = "Not recognized";
    }

    message = pCallbackData->pMessage;

    debugInfoStr = "Validation layer\n"
        + (std::string)"Severity: " + messageSeverityStr + "\n"
        + (std::string)"Type: " + messageTypeStr + "\n"
        + (std::string)"Message: " + message + "\n\n";

    std::cout << debugInfoStr;

    return VK_FALSE;
}