#pragma once
#include <iostream>
#include <vector>
#include <set>
#include <string>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include "Helpers/VulkanDevice.h"

class BaseSample
{
public:
	// Window
	GLFWwindow*							base_window = nullptr;
#define									WINDOW_WIDTH_SCALE 0.85
#define									WINDOW_HEIGHT_SCALE 0.75
	int									base_windowWidth = 0;
	int									base_windowHeight = 0;
	int									base_windowXPos = 0;
	int									base_windowYPos = 0;
	bool								base_framebufferResized = false;

	// Sample should set its requirements
	class SampleRequirements
	{
	public:
		// Used Vulkan API version (setting up by sample)
		uint32_t						base_apiVersion = VK_API_VERSION_1_1;
		// App title (setting up by sample)
		std::string						base_title = "No Title";
		// Enabled(required, setting up by sample) and supported instance extensions
		std::vector<const char*>		base_instanceEnabledExtensionsNames;
		// Enabled(required, setting up by sample) device extensions 
		std::vector<std::string>		base_deviceEnabledExtensionsNames;
		// Enabled(required, setting up by sample) device features
		VkPhysicalDeviceFeatures		base_deviceEnabledFeatures{};
		// Required queue family types(setting up by sample)
		VkQueueFlags					base_requiredQueueFamilyTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
	};
	SampleRequirements base_sampleRequirements;

	// Instance
	VkInstance							base_instance = VK_NULL_HANDLE;
	std::vector<VkExtensionProperties>	base_instanceSupportedExtensions;
	
	// Debug
	VkDebugUtilsMessengerEXT			base_debugMessenger = VK_NULL_HANDLE;

	// Surface
	VkSurfaceKHR						base_surface = VK_NULL_HANDLE;

	// Current Vulkan device
	VulkanDevice*						base_vulkanDevice = nullptr;
	// Default graphics queue
	VkQueue								base_graphicsQueue;
	// Default graphics queue
	VkQueue								base_presentQueue;
public:
	BaseSample();
	virtual ~BaseSample();

	void initVulkan();

	void initGlfw();

	void createWindow();

	void createInstance();

	void setupDebugMessenger();

	void createSurface();

	// Init VulkanDevice:
	// Pick physical device
	// Create logical device
	// Setting up queues
	void prepareDevice();

	// Redefined by the sample, called when checking the physical device
	// It should check the availability of the required features and mark them as enabled
	// Return FALSE if the device does not meet the requirements
	// Return TRUE if the device is suitable
	virtual bool getEnabledFeatures(VkPhysicalDevice physicalDevice);

	void finishVulkan();

	std::vector<const char*> getBaseRequiredInstanceExtensionsNames();

	// Returns a list of extensions not supported by the instance
	std::vector<std::string> getNotSupportedInstanceExtensionsNames();
};

