#include "BaseSample.h"
#include "ErrorInfo/ErrorInfo.h"
#include "ErrorInfo/ValidationLayers.h"
//#include "Helpers/VulkanDevice.h"

BaseSample::BaseSample()
{
}

BaseSample::~BaseSample()
{
}

static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
	BaseSample* app = reinterpret_cast<BaseSample*>(glfwGetWindowUserPointer(window));
	app->base_framebufferResized = true;
}

void BaseSample::initVulkan()
{
	initGlfw();
	createWindow();
	createInstance();
	if (ValidationLayers::enabled) {
		setupDebugMessenger();
	}
	createSurface();
	prepareDevice();
}

void BaseSample::finishVulkan()
{	
	if (base_vulkanDevice) {
		delete base_vulkanDevice;
	}
	vkDestroySurfaceKHR(base_instance, base_surface, nullptr);
	if (ValidationLayers::enabled) {
		ValidationLayers::DestroyDebugUtilsMessengerEXT(base_instance, base_debugMessenger, nullptr);
	}
	vkDestroyInstance(base_instance, nullptr);
	glfwDestroyWindow(base_window);
	glfwTerminate();
}

void BaseSample::initGlfw()
{
	//Initializing glfw
	if (!glfwInit()) {
		throw MakeErrorInfo("GLFW: initialization failed!");
	}

	//glfw should not create any contexts
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	if (!glfwVulkanSupported()) {
		throw MakeErrorInfo("GLFW: Vulkan not supported!");
	}
}

void BaseSample::createWindow()
{
	GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
	if (primaryMonitor == NULL) {
		throw MakeErrorInfo("GLFW: getting primary monitor failed!");
	}

	const GLFWvidmode* videoMode = glfwGetVideoMode(primaryMonitor);

	base_windowWidth = int(videoMode->width * WINDOW_WIDTH_SCALE);
	base_windowHeight = int(videoMode->height * WINDOW_HEIGHT_SCALE);
	base_windowXPos = (videoMode->width - base_windowWidth) / 2;
	base_windowYPos = (videoMode->height - base_windowHeight) / 2;

	base_window = glfwCreateWindow(base_windowWidth, base_windowHeight, base_sampleRequirements.base_title.c_str(), nullptr, nullptr);
	if (!base_window) {
		throw MakeErrorInfo("GLFW: window creation failed!");
	}

	glfwSetWindowPos(base_window, base_windowXPos, base_windowYPos);
	glfwSetWindowUserPointer(base_window, this);
	glfwSetFramebufferSizeCallback(base_window, framebufferResizeCallback);
}

void BaseSample::createInstance()
{
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = base_sampleRequirements.base_title.c_str();
	appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
	appInfo.apiVersion = base_sampleRequirements.base_apiVersion;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	std::vector<const char*> baseRequiredExtensionsNames = getBaseRequiredInstanceExtensionsNames();
	//instanceEnabledExtensions should already contain the names of the required extensions setted from sample
	for (size_t i = 0; i < base_sampleRequirements.base_instanceEnabledExtensionsNames.size(); ++i) {
		baseRequiredExtensionsNames.push_back(base_sampleRequirements.base_instanceEnabledExtensionsNames[i]);
	}
	base_sampleRequirements.base_instanceEnabledExtensionsNames = baseRequiredExtensionsNames;

	createInfo.enabledExtensionCount = base_sampleRequirements.base_instanceEnabledExtensionsNames.size();
	createInfo.ppEnabledExtensionNames = base_sampleRequirements.base_instanceEnabledExtensionsNames.data();

	//Setting up validation layers
	if (ValidationLayers::enabled && !ValidationLayers::checkSupport()) {
		throw MakeErrorInfo("Required validation layers are not available!");
	}
	else if (ValidationLayers::enabled && ValidationLayers::checkSupport()) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(ValidationLayers::validationLayers.size());
		createInfo.ppEnabledLayerNames = ValidationLayers::validationLayers.data();
	}

	//Get supported instance extensions
	uint32_t instanceSupportedExtensionsCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &instanceSupportedExtensionsCount, nullptr);
	base_instanceSupportedExtensions.resize(instanceSupportedExtensionsCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &instanceSupportedExtensionsCount, base_instanceSupportedExtensions.data());

	//Create instance
	VkResult result = vkCreateInstance(&createInfo, nullptr, &base_instance);
	if (result != VK_SUCCESS && result != VK_ERROR_EXTENSION_NOT_PRESENT) {
		throw MakeErrorInfo("Instance creation failed!");
	}
	else if (result == VK_ERROR_EXTENSION_NOT_PRESENT)
	{
		std::string message = "Instance not support required extensions!\n";

		std::vector<std::string> notSupportedInstanceExtensionsNames = getNotSupportedInstanceExtensionsNames();
		const size_t notSupportedInstanceExtensionsCount = notSupportedInstanceExtensionsNames.size();
		for (size_t i = 0; i < notSupportedInstanceExtensionsCount; ++i) {
			message.append(notSupportedInstanceExtensionsNames[i]);
			if (i != notSupportedInstanceExtensionsCount - 1) {
				message.append("\n");
			}
		}
		throw MakeErrorInfo(message.c_str());
	}
}

std::vector<const char*> BaseSample::getBaseRequiredInstanceExtensionsNames()
{
	uint32_t glfwExtensionsCount = 0;
	const char** glfwExtensionsNames = nullptr;
	
	//Requesting the necessary GLFW extensions to create an instance
	glfwExtensionsNames = glfwGetRequiredInstanceExtensions(&glfwExtensionsCount);
	std::vector<const char*> requiredExtensions(glfwExtensionsCount);
	for (size_t i = 0; i < glfwExtensionsCount; ++i) {
		requiredExtensions[i] = glfwExtensionsNames[i];
	}
	
	//Add debug extention
	if (ValidationLayers::enabled) {
		requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
	return requiredExtensions;
}

std::vector<std::string> BaseSample::getNotSupportedInstanceExtensionsNames()
{
	//Finding not supported instance extensions by names
	std::set<std::string> notSupportedInstanceExtensionsNames;
	for (auto& extensionName : base_sampleRequirements.base_instanceEnabledExtensionsNames) {
		notSupportedInstanceExtensionsNames.insert(extensionName);
	}
	for (auto& extension : base_instanceSupportedExtensions) {
		notSupportedInstanceExtensionsNames.erase(extension.extensionName);
	}
	
	std::vector<std::string> notSupportedInstanceExtensionsNamesVector;
	for (auto& extensionName : notSupportedInstanceExtensionsNames) {
		notSupportedInstanceExtensionsNamesVector.push_back(extensionName);
	}
	return notSupportedInstanceExtensionsNamesVector;
}

void BaseSample::setupDebugMessenger()
{
	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	//Severity levels to be processed
	createInfo.messageSeverity =
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	//Types to be processed
	createInfo.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	//Callback function
	createInfo.pfnUserCallback = ValidationLayers::debugCallback;
	createInfo.pUserData = nullptr; // Optional
	
	if (ValidationLayers::CreateDebugUtilsMessengerEXT(base_instance, &createInfo, nullptr, &base_debugMessenger) != VK_SUCCESS) {
		throw MakeErrorInfo("Debug messenger creation failed!");
	}
}

void BaseSample::createSurface()
{
	if (glfwCreateWindowSurface(base_instance, base_window, nullptr, &base_surface) != VK_SUCCESS) {
		throw MakeErrorInfo("Failed to create window surface!");
	}
}

void BaseSample::prepareDevice()
{
	// Get physical devices with Vulkan support
	uint32_t physicalDevicesCount = 0;
	vkEnumeratePhysicalDevices(base_instance, &physicalDevicesCount, nullptr);
	if (physicalDevicesCount == 0) {
		throw MakeErrorInfo("Failed to find GPUs with Vulkan support!");
	}
	std::vector<VkPhysicalDevice> physicalDevices(physicalDevicesCount);
	vkEnumeratePhysicalDevices(base_instance, &physicalDevicesCount, physicalDevices.data());

	// Search a suitable device 
	for (auto& physicalDevice : physicalDevices) {
		// Sample can check features
		if (!getEnabledFeatures(physicalDevice)) {
			continue;
		}
		// Check sample required device extensions
		VulkanDevice currentCheckedDevice(physicalDevice);
		if (!currentCheckedDevice.checkExtensionsSupport(base_sampleRequirements.base_deviceEnabledExtensionsNames)) {
			continue;
		}

		// If device is suitable
		base_vulkanDevice = new VulkanDevice(physicalDevice);
		break;
	}
	// We not found a suitable device?
	if (!base_vulkanDevice) {
		throw MakeErrorInfo("Failed to find a suitable device!");
	}

	// Find required queue families indices 
	base_vulkanDevice->getQueueFamilyIndices(base_sampleRequirements.base_requiredQueueFamilyTypes, base_surface);

	// Create logical device
	base_vulkanDevice->createLogicalDevice(base_sampleRequirements.base_deviceEnabledFeatures, base_sampleRequirements.base_deviceEnabledExtensionsNames);

	// Get device queues
	// Get graphics queue
	if (base_vulkanDevice->queueFamilyIndices.graphics.has_value()) {
		vkGetDeviceQueue(base_vulkanDevice->logicalDevice, base_vulkanDevice->queueFamilyIndices.graphics.value(), 0, &base_graphicsQueue);
	}
	// Get compute queue
	if (base_vulkanDevice->queueFamilyIndices.compute.has_value()) {
		vkGetDeviceQueue(base_vulkanDevice->logicalDevice, base_vulkanDevice->queueFamilyIndices.compute.value(), 0, &base_computeQueue);
	}
	// Get transfer queue
	if (base_vulkanDevice->queueFamilyIndices.transfer.has_value()) {
		vkGetDeviceQueue(base_vulkanDevice->logicalDevice, base_vulkanDevice->queueFamilyIndices.transfer.value(), 0, &base_transferQueue);
	}
	// Get present queue
	if (base_vulkanDevice->queueFamilyIndices.present.has_value()) {
		vkGetDeviceQueue(base_vulkanDevice->logicalDevice, base_vulkanDevice->queueFamilyIndices.present.value(), 0, &base_presentQueue);
	}
}

bool BaseSample::getEnabledFeatures(VkPhysicalDevice physicalDevice) { return false; }
