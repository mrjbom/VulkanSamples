#pragma warning(push, 0)
#define VMA_IMPLEMENTATION
/*
#define VMA_DEBUG_LOG(format, ...) do { \
       printf(format, __VA_ARGS__); \
       printf("\n"); \
   } while(false)
*/
#include "BaseSample.h"
#pragma warning(pop)
#include "ErrorInfo/ValidationLayers.h"

BaseSample::BaseSample()
{
}

BaseSample::~BaseSample()
{
}

static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    BaseSample* base = reinterpret_cast<BaseSample*>(glfwGetWindowUserPointer(window));
    base->base_framebufferResized = true;
    base->recreateSwapChain();
    base->draw();
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    BaseSample* base = reinterpret_cast<BaseSample*>(glfwGetWindowUserPointer(window));
    // The keyboard is used for typing in ImGui (for example, printing text), we must ignore keystrokes so that the camera would stand flat.
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) {
        // Stop moving
        base->base_camera.keys.up = false;
        base->base_camera.keys.down = false;
        base->base_camera.keys.left = false;
        base->base_camera.keys.right = false;
        return;
    }
    switch (key)
    {
    case GLFW_KEY_W:
        if (action == GLFW_PRESS) {
            base->base_camera.keys.up = true;
        }
        else if (action == GLFW_RELEASE) {
            base->base_camera.keys.up = false;
        }
        break;
    case GLFW_KEY_S:
        if (action == GLFW_PRESS) {
            base->base_camera.keys.down = true;
        }
        else if (action == GLFW_RELEASE) {
            base->base_camera.keys.down = false;
        }
        break;
    case GLFW_KEY_A:
        if (action == GLFW_PRESS) {
            base->base_camera.keys.left = true;
        }
        else if (action == GLFW_RELEASE) {
            base->base_camera.keys.left = false;
        }
        break;
    case GLFW_KEY_D:
        if (action == GLFW_PRESS) {
            base->base_camera.keys.right = true;
        }
        else if (action == GLFW_RELEASE) {
            base->base_camera.keys.right = false;
        }
        break;
    case GLFW_KEY_LEFT_SHIFT:
        if (action == GLFW_PRESS) {
            base->base_camera.keys.shift = true;
        }
        else if (action == GLFW_RELEASE) {
            base->base_camera.keys.shift = false;
        }
        break;
    }
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    BaseSample* base = reinterpret_cast<BaseSample*>(glfwGetWindowUserPointer(window));
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double xpos, ypos;
        glfwGetCursorPos(base->base_window, &xpos, &ypos);
        base->base_cursorPos.x = (float)xpos;
        base->base_cursorPos.y = (float)ypos;
    }
}

static void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos)
{
    BaseSample* base = reinterpret_cast<BaseSample*>(glfwGetWindowUserPointer(window));
    if (glfwGetMouseButton(base->base_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        float dx, dy;
        dx = (float)xpos - base->base_cursorPos.x;
        dy = (float)ypos - base->base_cursorPos.y;
        base->base_cursorPos.x = (float)xpos;
        base->base_cursorPos.y = (float)ypos;
        // The mouse moves over the ImGui window, we want to ignore the mouse so that the camera would stand
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse) {
            return;
        }
        
        // Around the Y-axis(horizontal)
        base->base_camera.rotate(glm::vec3(0.0f, dx * base->base_mouseSensitivity.x, 0.0f));
        // Around the X-axis(vertical)
        base->base_camera.rotate(glm::vec3(-dy * base->base_mouseSensitivity.y, 0.0f, 0.0f));
    }
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    BaseSample* base = reinterpret_cast<BaseSample*>(glfwGetWindowUserPointer(window));
    float translateDistance = (float)yoffset * base->base_mouseScrollSensitivity;
    base->base_camera.translateDistance(-translateDistance);
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
    initVma();
}

void BaseSample::prepare()
{
    createSwapChain();
    createDepthImage();
    createRenderPass();
    createFramebuffers();
    createCommandPoolGraphics();
    createCommandBuffersGraphics();
    createSyncObjects();
    imguiUI.initImGui(
        base_instance,
        base_vulkanDevice,
        base_graphicsQueue,
        base_vulkanSwapChain->surfaceSupportDetails.capabilities.minImageCount,
        base_vulkanSwapChain->images.size(),
        base_vulkanSwapChain,
        base_window,
        BASE_MAX_FRAMES_IN_FLIGHT
    );
}

void BaseSample::renderLoop()
{
    while (!glfwWindowShouldClose(base_window)) {
        std::chrono::time_point<std::chrono::steady_clock> startTime = std::chrono::steady_clock::now();
        glfwPollEvents();
        std::chrono::time_point<std::chrono::steady_clock> endTime = std::chrono::steady_clock::now();
        base_lastEventsPoolTime = std::chrono::duration<double, std::chrono::milliseconds::period>(endTime - startTime).count();

        nextFrame();
    }
}

void BaseSample::nextFrame()
{
    draw();
}

void BaseSample::prepareFrame()
{
    vkWaitForFences(base_vulkanDevice->logicalDevice, 1, &base_inFlightFences[base_currentFrameIndex], VK_TRUE, UINT64_MAX);

    // Calculating frametime
    std::chrono::time_point<std::chrono::steady_clock> currentTime = std::chrono::steady_clock::now();
    static std::chrono::time_point<std::chrono::steady_clock> prevTime(currentTime);
    base_frameTime = std::chrono::duration<double, std::chrono::milliseconds::period>(currentTime - prevTime).count();
    base_frameTime -= base_lastEventsPoolTime;
    prevTime = currentTime;

    VkResult result;
    result = vkAcquireNextImageKHR(base_vulkanDevice->logicalDevice, base_vulkanSwapChain->swapChain, UINT64_MAX, base_imageAvailableSemaphores[base_currentFrameIndex], VK_NULL_HANDLE, &base_currentImageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapChain();
    }
    else if (result != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to acquire swap chain image!");
    }

    vkResetFences(base_vulkanDevice->logicalDevice, 1, &base_inFlightFences[base_currentFrameIndex]);
}

void BaseSample::draw() { }


void BaseSample::submitFrame()
{
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &base_renderFinishedSemaphores[base_currentFrameIndex];

    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &base_vulkanSwapChain->swapChain;
    presentInfo.pImageIndices = &base_currentImageIndex;
    VkResult result;
    result = vkQueuePresentKHR(base_presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || base_framebufferResized) {
        base_framebufferResized = false;
        recreateSwapChain();
        return;
    }
    else if (result != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to present swap chain image!");
    }
    base_currentFrameIndex = (base_currentFrameIndex + 1) % BASE_MAX_FRAMES_IN_FLIGHT;
}

void BaseSample::recreateSwapChain()
{
    glfwGetFramebufferSize(base_window, &base_windowWidth, &base_windowHeight);
    // Is window minimized
    while (base_windowWidth == 0 || base_windowHeight == 0) {
        glfwGetFramebufferSize(base_window, &base_windowWidth, &base_windowHeight);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(base_vulkanDevice->logicalDevice);

    // Swap chain
    if (base_vulkanSwapChain) {
        delete base_vulkanSwapChain;
    }
    createSwapChain();

    // ImGui resources must be recreated
    imguiUI.resize(base_vulkanSwapChain);

    // Depth image
    if (base_depthImageView) {
        vkDestroyImageView(base_vulkanDevice->logicalDevice, base_depthImageView, nullptr);
    }
    if (base_depthImage) {
        vmaDestroyImage(base_vmaAllocator, base_depthImage, base_depthImageAllocation);
    }
    createDepthImage();

    // Framebuffer
    for (auto& swapChainFramebuffer : base_swapChainFramebuffers) {
        vkDestroyFramebuffer(base_vulkanDevice->logicalDevice, swapChainFramebuffer, nullptr);
    }
    createFramebuffers();
}

void BaseSample::finishVulkan()
{
    vkDeviceWaitIdle(base_vulkanDevice->logicalDevice);

    // ImGuiUI resources
    imguiUI.cleanupImGui();
    // Synchronization objects
    for (auto& imageAvailableSemaphore : base_imageAvailableSemaphores) {
        vkDestroySemaphore(base_vulkanDevice->logicalDevice, imageAvailableSemaphore, nullptr);
    }
    for (auto& renderFinishedSemaphore : base_renderFinishedSemaphores) {
        vkDestroySemaphore(base_vulkanDevice->logicalDevice, renderFinishedSemaphore, nullptr);
    }
    for (auto& inFlightFence : base_inFlightFences) {
        vkDestroyFence(base_vulkanDevice->logicalDevice, inFlightFence, nullptr);
    }
    // Graphics command pool
    if (base_commandPoolGraphics) {
        vkDestroyCommandPool(base_vulkanDevice->logicalDevice, base_commandPoolGraphics, nullptr);
    }
    // Framebuffers
    for (auto& swapChainFramebuffer : base_swapChainFramebuffers) {
        vkDestroyFramebuffer(base_vulkanDevice->logicalDevice, swapChainFramebuffer, nullptr);
    }
    // Renderpass
    if (base_renderPass) {
        vkDestroyRenderPass(base_vulkanDevice->logicalDevice, base_renderPass, nullptr);
    }
    // Depth image
    if (base_depthImageView) {
        vkDestroyImageView(base_vulkanDevice->logicalDevice, base_depthImageView, nullptr);
    }
    if (base_depthImage) {
        vmaDestroyImage(base_vmaAllocator, base_depthImage, base_depthImageAllocation);
    }
    // Swap chain
    if (base_vulkanSwapChain) {
        delete base_vulkanSwapChain;
    }

    if (base_vmaAllocator) {
        vmaDestroyAllocator(base_vmaAllocator);
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

    base_monitor = primaryMonitor;

    const GLFWvidmode* videoMode = glfwGetVideoMode(primaryMonitor);

    base_windowWidth = int(videoMode->width * WINDOW_WIDTH_SCALE);
    base_windowHeight = int(videoMode->height * WINDOW_HEIGHT_SCALE);
    base_windowXPos = (videoMode->width - base_windowWidth) / 2;
    base_windowYPos = (videoMode->height - base_windowHeight) / 2;

    base_window = glfwCreateWindow(base_windowWidth, base_windowHeight, base_title.c_str(), nullptr, nullptr);
    if (!base_window) {
        throw MakeErrorInfo("GLFW: window creation failed!");
    }

    // Hand minimization of height or width to 0 is badly handled.
    // Full minimization with the minimize button is handled correctly(window size in this case is (0;0)).
    glfwSetWindowSizeLimits(base_window, 1, 1, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwSetWindowPos(base_window, base_windowXPos, base_windowYPos);
    glfwSetWindowUserPointer(base_window, this);
    glfwSetFramebufferSizeCallback(base_window, framebufferResizeCallback);
    glfwSetKeyCallback(base_window, keyCallback);
    glfwSetMouseButtonCallback(base_window, mouseButtonCallback);
    glfwSetCursorPosCallback(base_window, cursorPositionCallback);
    glfwSetScrollCallback(base_window, scrollCallback);
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

void BaseSample::initVma()
{
    VmaAllocatorCreateInfo allocatorCreateInfo{};
    allocatorCreateInfo.physicalDevice = base_vulkanDevice->physicalDevice;
    allocatorCreateInfo.device = base_vulkanDevice->logicalDevice;
    allocatorCreateInfo.instance = base_instance;
    allocatorCreateInfo.vulkanApiVersion = base_sampleInstanceRequirements.base_instanceApiVersion;

    vmaCreateAllocator(&allocatorCreateInfo, &base_vmaAllocator);
}

bool BaseSample::getEnabledFeatures(VkPhysicalDevice physicalDevice) { return false; }

void BaseSample::createSwapChain()
{
    // Create swap chain object
    base_vulkanSwapChain = new VulkanSwapChain(base_vulkanDevice, base_surface, base_window);
    
    // Create swap chain
    base_vulkanSwapChain->createSwapChain(base_sampleSwapChainRequirements.base_swapChainPrefferedFormat, base_sampleSwapChainRequirements.base_swapChainPrefferedPresentMode);
}

void BaseSample::createDepthImage()
{
    if (!vulkanTools::getSupportedDepthFormat(this->base_vulkanDevice->physicalDevice, &base_depthFormat)) {
        throw MakeErrorInfo("Failed to find supported depth format!");
    }
    // Fill image info
    VkImageCreateInfo imageCreateInfo{};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = base_depthFormat;
    imageCreateInfo.extent.width = base_vulkanSwapChain->surfaceExtent.width;
    imageCreateInfo.extent.height = base_vulkanSwapChain->surfaceExtent.height;
    imageCreateInfo.extent.depth = 1;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // Fill VMA allocation info
    VmaAllocationCreateInfo allocationCreateInfo{};
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocationCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    if(vmaCreateImage(base_vmaAllocator, &imageCreateInfo, &allocationCreateInfo, &base_depthImage, &base_depthImageAllocation, &base_depthImageAllocationInfo) != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to create depth image");
    }

    // Create image view
    VkImageViewCreateInfo imageViewCreateInfo{};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.image = base_depthImage;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = base_depthFormat;
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(base_vulkanDevice->logicalDevice, &imageViewCreateInfo, nullptr, &base_depthImageView) != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to create depth image view!");
    }
}

void BaseSample::createRenderPass()
{
    // Attachments descriptions
    std::vector<VkAttachmentDescription> attachmentsDescriptions;
    // attachment description [0] - swap chain image
    attachmentsDescriptions.push_back({ });
    attachmentsDescriptions.back().format = base_vulkanSwapChain->surfaceFormat.format;
    attachmentsDescriptions.back().samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentsDescriptions.back().loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentsDescriptions.back().storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentsDescriptions.back().stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentsDescriptions.back().stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentsDescriptions.back().initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentsDescriptions.back().finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    // attachment description [1] - depth image
    attachmentsDescriptions.push_back({ });
    attachmentsDescriptions.back().format = base_depthFormat;
    attachmentsDescriptions.back().samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentsDescriptions.back().loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentsDescriptions.back().storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentsDescriptions.back().stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentsDescriptions.back().stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentsDescriptions.back().initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentsDescriptions.back().finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Attachments references
    std::vector<VkAttachmentReference> attachmentsReferences;
    // attachments references [0] - swap chain image
    attachmentsReferences.push_back({ });
    attachmentsReferences.back().attachment = 0; // Index in the attachmentsDescriptions array
    // The layout specifies which layout we would like the attachment to have during a subpass that uses this reference
    // Vulkan will automatically transition the attachment to this layout when the subpass is started
    attachmentsReferences.back().layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    // attachments references [1] - depth image
    attachmentsReferences.push_back({ });
    attachmentsReferences.back().attachment = 1;
    attachmentsReferences.back().layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Subpasses
    std::vector<VkSubpassDescription> subpassDescriptions;
    // subbpass [0]
    subpassDescriptions.push_back({ });
    subpassDescriptions.back().pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescriptions.back().colorAttachmentCount = 1;
    subpassDescriptions.back().pColorAttachments = &attachmentsReferences[0];
    subpassDescriptions.back().pDepthStencilAttachment = &attachmentsReferences[1];

    // Subpass dependencies
    std::vector<VkSubpassDependency> subpassDependencies;
    subpassDependencies.push_back({ });
    // Swap chain image color attachment
    // Now, putting it all together:
    // the semaphore signal from vkAcquireNextImage makes the swapchain image available from the read of the presentation engine.
    // The semaphore wait in vkQueueSubmit makes the swapchain image visible to all commands in the Batch limited to COLOR_ATTACHMENT_OUTPUT.
    // The VkSubpassDependency chains to that semaphore wait. The image is still visible to, so no additional memory dependency is needed and so our .srcAccessMask is 0.
    // The layout transition writes the image and makes it (implicitly) available from the layout transition and visible to whatever the .dst* was provided to the VkSubpassDependency.
    subpassDependencies.back().srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependencies.back().dstSubpass = 0;
    subpassDependencies.back().srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies.back().srcAccessMask = 0;
    subpassDependencies.back().dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies.back().dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    // Why .dstAccessMask not 0?
    // The load operation for each sample in an attachment happens-before any recorded command which accesses the sample in the first subpass where the attachment is used. 
    // Load operations for attachments with a color format execute in the VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT pipeline stage.
    // VK_ATTACHMENT_LOAD_OP_LOAD [...] For attachments with a color format, this uses the access type VK_ACCESS_COLOR_ATTACHMENT_READ_BIT.
    //VK_ATTACHMENT_LOAD_OP_CLEAR(or VK_ATTACHMENT_LOAD_OP_DONT_CARE) [...] For attachments with a color format, this uses the access type VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT.

    // Depth attachment image subpass depencency
    // We have to wait until another frame (rendered in parallel and using the same depth image)
    // finishes its operations with the depth image
    subpassDependencies.push_back({ });
    subpassDependencies.back().srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependencies.back().dstSubpass = 0;
    subpassDependencies.back().srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    subpassDependencies.back().srcAccessMask = 0;
    subpassDependencies.back().dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    subpassDependencies.back().dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // Create renderpass
    VkRenderPassCreateInfo renderpassCreateInfo{};
    renderpassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderpassCreateInfo.attachmentCount = attachmentsDescriptions.size();
    renderpassCreateInfo.pAttachments = attachmentsDescriptions.data();
    renderpassCreateInfo.subpassCount = subpassDescriptions.size();
    renderpassCreateInfo.pSubpasses = subpassDescriptions.data();
    renderpassCreateInfo.dependencyCount = subpassDependencies.size();
    renderpassCreateInfo.pDependencies = subpassDependencies.data();

    VkResult result;
    result = vkCreateRenderPass(base_vulkanDevice->logicalDevice, &renderpassCreateInfo, nullptr, &base_renderPass);
    if (result != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to create renderpass!");
    }
}

void BaseSample::createFramebuffers()
{
    base_swapChainFramebuffers.resize(base_vulkanSwapChain->imagesViews.size());

    for (size_t i = 0; i < base_vulkanSwapChain->imagesViews.size(); i++) {
        std::vector<VkImageView> attachments = {
            base_vulkanSwapChain->imagesViews[i],
            base_depthImageView
        };

        VkFramebufferCreateInfo framebufferCreateInfo{};
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.renderPass = base_renderPass;
        framebufferCreateInfo.attachmentCount = attachments.size();
        framebufferCreateInfo.pAttachments = attachments.data();
        framebufferCreateInfo.width = base_vulkanSwapChain->surfaceExtent.width;
        framebufferCreateInfo.height = base_vulkanSwapChain->surfaceExtent.height;
        framebufferCreateInfo.layers = 1;

        if (vkCreateFramebuffer(base_vulkanDevice->logicalDevice, &framebufferCreateInfo, nullptr, &base_swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to create framebuffers!");
        }
    }
}

void BaseSample::createCommandPoolGraphics()
{
    VkCommandPoolCreateInfo commandPoolCreateInfo{};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = base_vulkanDevice->queueFamilyIndices.graphics.value();

    if (vkCreateCommandPool(base_vulkanDevice->logicalDevice, &commandPoolCreateInfo, nullptr, &base_commandPoolGraphics) != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to create graphics command pool!");
    }
}

void BaseSample::createCommandBuffersGraphics()
{
    base_commandBuffersGraphics.resize(BASE_MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = base_commandPoolGraphics;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)base_commandBuffersGraphics.size();

    //Allocate command buffers from command pool
    if (vkAllocateCommandBuffers(base_vulkanDevice->logicalDevice, &allocInfo, base_commandBuffersGraphics.data()) != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to allocate command buffers!");
    }
}

void BaseSample::createSyncObjects()
{
    base_imageAvailableSemaphores.resize(BASE_MAX_FRAMES_IN_FLIGHT);
    base_renderFinishedSemaphores.resize(BASE_MAX_FRAMES_IN_FLIGHT);
    base_inFlightFences.resize(BASE_MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < BASE_MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(base_vulkanDevice->logicalDevice, &semaphoreCreateInfo, nullptr, &base_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(base_vulkanDevice->logicalDevice, &semaphoreCreateInfo, nullptr, &base_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(base_vulkanDevice->logicalDevice, &fenceCreateInfo, nullptr, &base_inFlightFences[i]) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to create synchronization objects for a frame!");
        }
    }
}

void BaseSample::setupSubmitInfo(uint32_t currentFrameIndex)
{
    base_submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    base_submitInfo.pWaitDstStageMask = base_submittingWaitStages.data();
    base_submitInfo.waitSemaphoreCount = 1;
    base_submitInfo.pWaitSemaphores = &base_imageAvailableSemaphores[currentFrameIndex];
    base_submitInfo.signalSemaphoreCount = 1;
    base_submitInfo.pSignalSemaphores = &base_renderFinishedSemaphores[currentFrameIndex];
}

void BaseSample::drawUI() {};