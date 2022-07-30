#include "BaseSample.h"
#include "ErrorInfo/ErrorInfo.h"
#include "ErrorInfo/ValidationLayers.h"

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

void BaseSample::prepare()
{
    createSwapChain();
    createRenderPass();
    createFramebuffers();
}

void BaseSample::finishVulkan()
{
    // Framebuffers
    for (auto& swapChainFramebuffer : base_swapChainFramebuffers) {
        vkDestroyFramebuffer(base_vulkanDevice->logicalDevice, swapChainFramebuffer, nullptr);
    }
    // Renderpass
    if (base_renderPass) {
        vkDestroyRenderPass(base_vulkanDevice->logicalDevice, base_renderPass, nullptr);
    }
    // Swap chain
    if (base_vulkanSwapChain) {
        delete base_vulkanSwapChain;
    }
    // Logical device
    if (base_vulkanDevice) {
        delete base_vulkanDevice;
    }
    // Surface
    vkDestroySurfaceKHR(base_instance, base_surface, nullptr);
    // Debug messenger
    if (ValidationLayers::enabled) {
        ValidationLayers::DestroyDebugUtilsMessengerEXT(base_instance, base_debugMessenger, nullptr);
    }
    // Instance
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

    base_window = glfwCreateWindow(base_windowWidth, base_windowHeight, base_title.c_str(), nullptr, nullptr);
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
    appInfo.pApplicationName = base_title.c_str();
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.apiVersion = base_sampleInstanceRequirements.base_instanceApiVersion;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    std::vector<const char*> baseRequiredExtensionsNames = getBaseRequiredInstanceExtensionsNames();
    //instanceEnabledExtensions should already contain the names of the required extensions setted from sample
    for (size_t i = 0; i < base_sampleInstanceRequirements.base_instanceEnabledExtensionsNames.size(); ++i) {
        baseRequiredExtensionsNames.push_back(base_sampleInstanceRequirements.base_instanceEnabledExtensionsNames[i]);
    }
    base_sampleInstanceRequirements.base_instanceEnabledExtensionsNames = baseRequiredExtensionsNames;

    createInfo.enabledExtensionCount = base_sampleInstanceRequirements.base_instanceEnabledExtensionsNames.size();
    createInfo.ppEnabledExtensionNames = base_sampleInstanceRequirements.base_instanceEnabledExtensionsNames.data();

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

        std::vector<std::string> notSupportedInstanceExtensionsNames = getInstanceNotSupportedExtensionsNames();
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

std::vector<std::string> BaseSample::getInstanceNotSupportedExtensionsNames()
{
    //Finding not supported instance extensions by names
    std::set<std::string> notSupportedInstanceExtensionsNames;
    for (const auto& extensionName : base_sampleInstanceRequirements.base_instanceEnabledExtensionsNames) {
        notSupportedInstanceExtensionsNames.insert(extensionName);
    }
    for (const auto& extension : base_instanceSupportedExtensions) {
        notSupportedInstanceExtensionsNames.erase(extension.extensionName);
    }
    
    std::vector<std::string> notSupportedInstanceExtensionsNamesVector;
    for (const auto& extensionName : notSupportedInstanceExtensionsNames) {
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
    //PHYSICAL DEVICE

    // Get physical devices with Vulkan support
    uint32_t physicalDevicesCount = 0;
    vkEnumeratePhysicalDevices(base_instance, &physicalDevicesCount, nullptr);
    if (physicalDevicesCount == 0) {
        throw MakeErrorInfo("Failed to find GPUs with Vulkan support!");
    }
    std::vector<VkPhysicalDevice> physicalDevices(physicalDevicesCount);
    vkEnumeratePhysicalDevices(base_instance, &physicalDevicesCount, physicalDevices.data());

    if (base_surface) {
        base_sampleDeviceRequirements.base_deviceEnabledExtensionsNames.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    // Search a suitable device 
    for (const auto& physicalDevice : physicalDevices) {
        // Sample can check features
        if (!getEnabledFeatures(physicalDevice)) {
            continue;
        }
        // Check sample required device extensions
        VulkanDevice currentCheckedDevice(physicalDevice);
        if (!currentCheckedDevice.checkExtensionsSupport(base_sampleDeviceRequirements.base_deviceEnabledExtensionsNames)) {
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

    //LOGICAL DEVICE

    // Find and save required queue families indices 
    base_vulkanDevice->findQueueFamilyIndices(base_sampleDeviceRequirements.base_deviceRequiredQueueFamilyTypes, base_surface);

    // Create and save logical device
    base_vulkanDevice->createLogicalDevice(base_sampleDeviceRequirements.base_deviceEnabledFeatures, base_sampleDeviceRequirements.base_deviceEnabledExtensionsNames);

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

void BaseSample::createSwapChain()
{
    // Create swap chain object
    base_vulkanSwapChain = new VulkanSwapChain(base_vulkanDevice, base_surface, base_window);
    
    // Create swap chain
    base_vulkanSwapChain->createSwapChain(base_sampleSwapChainRequirements.base_swapChainPrefferedFormat, base_sampleSwapChainRequirements.base_swapChainPrefferedPresentMode);
}

void BaseSample::createRenderPass()
{
    // Attachments descriptions
    std::vector<VkAttachmentDescription> attachmentsDescriptions;
    // attachment description [0] - swap chain image
    attachmentsDescriptions.push_back({});
    attachmentsDescriptions.back().format = base_vulkanSwapChain->swapChainFormat.format;
    attachmentsDescriptions.back().samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentsDescriptions.back().loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentsDescriptions.back().storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentsDescriptions.back().stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentsDescriptions.back().stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentsDescriptions.back().initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentsDescriptions.back().finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Attachments references
    std::vector<VkAttachmentReference> attachmentsReferences;
    // attachments references [0]
    attachmentsReferences.push_back({});
    attachmentsReferences.back().attachment = 0; // Index in the attachmentsDescriptions array
    // The layout specifies which layout we would like the attachment to have during a subpass that uses this reference
    // Vulkan will automatically transition the attachment to this layout when the subpass is started
    attachmentsReferences.back().layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Subpasses
    std::vector<VkSubpassDescription> subpassDescriptions;
    // subbpass [0]
    subpassDescriptions.push_back({});
    subpassDescriptions.back().pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescriptions.back().colorAttachmentCount = 1;
    subpassDescriptions.back().pColorAttachments = &attachmentsReferences[0];

    // Create renderpass
    VkRenderPassCreateInfo renderpassCreateInfo{};
    renderpassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderpassCreateInfo.attachmentCount = 1;
    renderpassCreateInfo.pAttachments = &attachmentsDescriptions[0];
    renderpassCreateInfo.subpassCount = subpassDescriptions.size();
    renderpassCreateInfo.pSubpasses = subpassDescriptions.data();

    VkResult result;
    result = vkCreateRenderPass(base_vulkanDevice->logicalDevice, &renderpassCreateInfo, nullptr, &base_renderPass);
    if (result != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to create renderpass!");
    }
}

void BaseSample::createFramebuffers()
{
    base_swapChainFramebuffers.resize(base_vulkanSwapChain->swapChainImagesViews.size());

    for (size_t i = 0; i < base_vulkanSwapChain->swapChainImagesViews.size(); i++) {
        VkImageView attachments[] = {
            base_vulkanSwapChain->swapChainImagesViews[i]
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = base_renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = base_vulkanSwapChain->swapChainExtent.width;
        framebufferInfo.height = base_vulkanSwapChain->swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(base_vulkanDevice->logicalDevice, &framebufferInfo, nullptr, &base_swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to create framebuffers!");
        }
    }
}
