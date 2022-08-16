#pragma once

#include <vector>
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <../imgui/imconfig.h>
#include <../imgui/imgui.h>
#include <../imgui/imgui_impl_glfw.h>
#include <../imgui/imgui_impl_vulkan.h>
#include "VulkanDevice.h"
#include "VulkanSwapChain.h"

class ImGuiUI
{
private:
    VkInstance instance;
    VulkanDevice* vulkanDevice = nullptr;
    VulkanSwapChain* vulkanSwapChain = nullptr;
    int maxFramesInFlight;
    VkDescriptorPool imguiDescriptorPool;
    VkCommandPool imguiCommandPool;
    std::vector<VkCommandBuffer> imguiCommandBuffers;
    std::vector<VkFramebuffer> imguiFramebuffers;
    VkRenderPass imguiRenderPass;
public:
    // Creating descriptor pool
    // Creating command pool
    // Allocating command buffers
    // Creating renderpass
    // Creating framebuffers
    void initImGui(VkInstance instance, VulkanDevice* vulkanDevice, VkQueue graphicsQueue, int minImageCount, int imageCount, VulkanSwapChain* vulkanSwapChain, GLFWwindow* window, int maxFramesInFlight);
    void cleanupImGui();
    void resize(VulkanSwapChain* vulkanSwapChain);
    void beginFrame();
    void endFrame();
    VkCommandBuffer recordAndGetCommandBuffer(int currentFrameIndex, int currentImageIndex);
private:
    void createDescriptorPool();
    void createCommandPool();
    void allocateCommandBuffers();
    void createRenderPass();
    void createFramebuffers();
};

