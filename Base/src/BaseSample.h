#pragma once
#include <iostream>
#include <vector>
#include <set>
#include <string>
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#pragma warning(push, 0)
#include <vk_mem_alloc.h>
#pragma warning(pop)
#include <Windows.h>
#include "ErrorInfo/ErrorInfo.h"
#include "Helpers/VulkanDevice.h"
#include "Helpers/VulkanSwapChain.h"
#include "Helpers/VulkanInitializers.hpp"
#include "Helpers/VulkanTools.h"
#include "Helpers/VulkanTexture.h"
#include "Helpers/Camera.hpp"
#include "Helpers/ImGuiUI.h"
#include "Helpers/UIOverlay.hpp"
#include "Helpers/VulkanglTFModel.h"

const std::string ASSETS_DATA_PATH = "../../data/";
const std::string ASSETS_DATA_SHADERS_PATH = "../../data/shaders/";

class BaseSample
{
public:
    GLFWmonitor*                        base_monitor = nullptr;
    GLFWwindow*                         base_window = nullptr;
#define                                 WINDOW_WIDTH_SCALE 0.85
#define                                 WINDOW_HEIGHT_SCALE 0.75
    int                                 base_windowWidth = 0;
    int                                 base_windowHeight = 0;
    int                                 base_windowXPos = 0;
    int                                 base_windowYPos = 0;
    bool                                base_framebufferResized = false;
    // Frametime in milliseconds
    double                              base_frameTime = 0;
    // Time it took to get glfw events(glfwPoolEvents()), is used to fix for increased frametime
    double                              base_lastEventsPoolTime = 0;

    Camera                              base_camera;
    glm::vec2                           base_cursorPos{};
    // Rotate camera by 1 degree per X pixels passed by the cursor
    glm::vec2                           base_mouseSensitivity{ 1.0f / 6, 1.0f / 6 };
    float                               base_mouseScrollSensitivity = 0.25f;

    // Sample should set its requirements

    // App title (setting up by sample)
    std::string                         base_title = "No Title";

    class SampleInstanceRequirements
    {
    public:
        // Used Vulkan API version (setting up by sample)
        uint32_t                        base_instanceApiVersion = VK_API_VERSION_1_1;
        // Enabled(required, setting up by sample) and supported instance extensions
        std::vector<const char*>        base_instanceEnabledExtensionsNames;
    };
    SampleInstanceRequirements base_sampleInstanceRequirements;

    class SampleDeviceRequirements
    {
    public:
        // Enabled(required, setting up by sample) device extensions 
        std::vector<std::string>        base_deviceEnabledExtensionsNames;
        // Enabled(required, setting up by sample) device features
        VkPhysicalDeviceFeatures        base_deviceEnabledFeatures{};
        // Required queue family types(setting up by sample)
        VkQueueFlags                    base_deviceRequiredQueueFamilyTypes = VK_QUEUE_GRAPHICS_BIT;
    };
    SampleDeviceRequirements base_sampleDeviceRequirements;

    class SampleSwapChainRequirements
    {
    public:
        // Preferred surface format, it will be selected if supported, otherwise another will be selected
        VkSurfaceFormatKHR              base_swapChainPrefferedFormat{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        // Preferred surface present mode, it will be selected if supported, otherwise another will be selected
        VkPresentModeKHR                base_swapChainPrefferedPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    };
    SampleSwapChainRequirements base_sampleSwapChainRequirements;

    // Instance
    VkInstance                          base_instance = VK_NULL_HANDLE;
    std::vector<VkExtensionProperties>  base_instanceSupportedExtensions;
    
    // Debug
    VkDebugUtilsMessengerEXT            base_debugMessenger = VK_NULL_HANDLE;

    // Current Vulkan device
    VulkanDevice*                       base_vulkanDevice = nullptr;
    // Default graphics queue
    VkQueue                             base_graphicsQueue = VK_NULL_HANDLE;
    // Default compute queue
    VkQueue                             base_computeQueue = VK_NULL_HANDLE;
    // Default transfer queue
    VkQueue                             base_transferQueue = VK_NULL_HANDLE;
    // Default present queue
    VkQueue                             base_presentQueue = VK_NULL_HANDLE;

    // Surface
    VkSurfaceKHR                        base_surface = VK_NULL_HANDLE;

    // SwapChain
    VulkanSwapChain*                    base_vulkanSwapChain = nullptr;

    // General render pass
    VkRenderPass                        base_renderPass = VK_NULL_HANDLE;

    // Framebuffers for swap chain images
    std::vector<VkFramebuffer>          base_swapChainFramebuffers;

    // Vulkan Memory Allocator
    VmaAllocator                        base_vmaAllocator;

    // Depth image and VMA allocation data
    VkFormat                            base_depthFormat = VK_FORMAT_UNDEFINED;
    VkImage                             base_depthImage = VK_NULL_HANDLE;
    VmaAllocation                       base_depthImageAllocation = 0;
    VmaAllocationInfo                   base_depthImageAllocationInfo{};
    VkImageView                         base_depthImageView = VK_NULL_HANDLE;

    // Command pool for graphics commands(use graphical queue family)
    // Spec says: All command buffers allocated from this command pool must be submitted on queues from the same queue family
    VkCommandPool                       base_commandPoolGraphics = VK_NULL_HANDLE;

    // Command buffers for graphics operation allocated from graphics command pool
    std::vector<VkCommandBuffer>        base_commandBuffersGraphics;

    // The number of frames that can be rendered in parallel
    #define BASE_MAX_FRAMES_IN_FLIGHT 2

    // Current frame index(NOT CURRENT SWAP CHAIN IMAGE INDEX)
    uint32_t base_currentFrameIndex = 0;

    // Current swap chain image index
    uint32_t base_currentImageIndex = 0;

    // The image from the swap chain was successfully aquired
    std::vector<VkSemaphore> base_imageAvailableSemaphores;

    // The command buffer with graphical operations has been successfully executed
    std::vector<VkSemaphore> base_renderFinishedSemaphores;

    // We can start rendering the frame(and record to command buffer)
    std::vector<VkFence> base_inFlightFences;

    // Submit info struct for graphics command buffers submitting
    VkSubmitInfo base_submitInfo{};

    // Specify which stages to wait on before command buffer execution begins
    std::vector<VkPipelineStageFlags> base_submittingWaitStages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    // ImGuiUI object
    ImGuiUI imguiUI;
public:
    BaseSample();
    virtual ~BaseSample();

    void initVulkan();

    // Always redefined by the sample
    virtual void prepare();

    void renderLoop();

    void nextFrame();

    // Acquire image
    void prepareFrame();

    // Always redefined by the sample
    virtual void draw();

    // Present image to the swap chain
    void submitFrame();

    // Recreate swapchain(window resize)
    void recreateSwapChain();

    void initGlfw();

    void createWindow();

    void createInstance();

    std::vector<const char*> getBaseRequiredInstanceExtensionsNames();

    // Returns a list of extensions not supported by the instance
    std::vector<std::string> getInstanceNotSupportedExtensionsNames();

    void setupDebugMessenger();

    void createSurface();

    // Init VulkanDevice
    // Pick physical device
    // Create logical device
    // Get queues
    void prepareDevice();

    // Init Vulkan Memory Allocator
    void initVma();

    // Always redefined by the sample, called when checking the physical device
    // It should check the availability of the required features and mark them as enabled
    // Return FALSE if the device does not meet the requirements
    // Return TRUE if the device is suitable
    virtual bool getEnabledFeatures(VkPhysicalDevice physicalDevice);

    // Create swap chain
    // Retrieve swap chain images
    // Create image views
    void createSwapChain();

    // Create depth image
    void createDepthImage();

    // Create general render pass
    void createRenderPass();

    // Create framebuffers
    void createFramebuffers();

    // Create command pool for graphics command pool
    void createCommandPoolGraphics();

    // Allocate graphics command buffers from graphics command pool
    void createCommandBuffersGraphics();

    // Create syncronization objects for rendering
    void createSyncObjects();

    // Set up submit info structure (semaphores)
    // Semaphores will stay the same during application lifetime
    // Command buffer submission info is set by each example
    void setupSubmitInfo(uint32_t currentFrameIndex);

    virtual void drawUI();

    void finishVulkan();
};

#define EXAMPLE_MAIN(class_name)                                            \
int main(int argc, char* argv[])                                            \
{                                                                           \
    try                                                                     \
    {                                                                       \
        class_name* sample = new class_name();                              \
        sample->initVulkan();                                               \
        sample->prepare();                                                  \
        sample->renderLoop();                                               \
        sample->cleanup();                                                  \
        sample->finishVulkan();                                             \
        delete sample;                                                      \
    }                                                                       \
    catch (std::exception ex)                                               \
    {                                                                       \
        std::cout << "Exception: " << ex.what() << std::endl;               \
        return EXIT_FAILURE;                                                \
    }                                                                       \
    catch (ErrorInfo errorInfo)                                             \
    {                                                                       \
        std::string errInfoStr = "Exception\n"                              \
            + (std::string)"What: " + errorInfo.what + "\n"                 \
            + (std::string)"File: " + errorInfo.file + "\n"                 \
            + (std::string)"Line: " + errorInfo.line + "\n";                \
        std::cout << errInfoStr;                                            \
        return EXIT_FAILURE;                                                \
    }                                                                       \
    return EXIT_SUCCESS;                                                    \
}
