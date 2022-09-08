#include "ImGuiUI.h"

#include "VulkanSwapChain.h"
#include "VulkanInitializers.hpp"

void ImGuiUI::initImGui(VkInstance instance, VulkanDevice* vulkanDevice, VkQueue graphicsQueue, int minImageCount, int imageCount, VulkanSwapChain* vulkanSwapChain, GLFWwindow* window, int maxFramesInFlight)
{
    this->instance = instance;
    this->vulkanDevice = vulkanDevice;
    this->vulkanSwapChain = vulkanSwapChain;
    this->maxFramesInFlight = maxFramesInFlight;
    this->createDescriptorPool();
    this->createCommandPool();
    this->allocateCommandBuffers();
    this->createRenderPass();
    this->createFramebuffers();

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    // Disable imgui.ini file
    io.IniFilename = nullptr;
    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_InitInfo implVulkanInitInfo{};
    implVulkanInitInfo.Instance = instance;
    implVulkanInitInfo.PhysicalDevice = vulkanDevice->physicalDevice;
    implVulkanInitInfo.Device = vulkanDevice->logicalDevice;
    implVulkanInitInfo.QueueFamily = vulkanDevice->queueFamilyIndices.graphics.value();
    implVulkanInitInfo.Queue = graphicsQueue;
    implVulkanInitInfo.PipelineCache = VK_NULL_HANDLE;
    implVulkanInitInfo.DescriptorPool = imguiDescriptorPool;
    implVulkanInitInfo.Subpass = 0;
    implVulkanInitInfo.MinImageCount = minImageCount;
    implVulkanInitInfo.ImageCount = imageCount;
    implVulkanInitInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    implVulkanInitInfo.Allocator = nullptr;
    implVulkanInitInfo.CheckVkResultFn = nullptr;
    ImGui_ImplVulkan_Init(&implVulkanInitInfo, imguiRenderPass);

    // Upload font textures to the GPU
    VkCommandBuffer uploadCommandBuffer = vulkanDevice->beginSingleTimeCommands(imguiCommandPool);
    ImGui_ImplVulkan_CreateFontsTexture(uploadCommandBuffer);
    vulkanDevice->endSingleTimeCommands(uploadCommandBuffer, graphicsQueue, imguiCommandPool);
}

void ImGuiUI::cleanupImGui()
{
    vkDeviceWaitIdle(vulkanDevice->logicalDevice);

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // Framebuffers
    for (auto& framebuffer : imguiFramebuffers) {
        vkDestroyFramebuffer(vulkanDevice->logicalDevice, framebuffer, nullptr);
    }
    // Renderpass
    vkDestroyRenderPass(vulkanDevice->logicalDevice, imguiRenderPass, nullptr);
    // Command pool
    vkDestroyCommandPool(vulkanDevice->logicalDevice, imguiCommandPool, nullptr);
    // Descriptor pool
    vkDestroyDescriptorPool(vulkanDevice->logicalDevice, imguiDescriptorPool, nullptr);
}

void ImGuiUI::resize(VulkanSwapChain* vulkanSwapChain)
{
    this->vulkanSwapChain = vulkanSwapChain;

    // Delete old framebuffers
    for (auto& framebuffer : imguiFramebuffers) {
        vkDestroyFramebuffer(vulkanDevice->logicalDevice, framebuffer, nullptr);
    }

    this->createFramebuffers();

    ImGui_ImplVulkan_SetMinImageCount(vulkanSwapChain->surfaceSupportDetails.capabilities.minImageCount);
}

void ImGuiUI::beginFrame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiUI::endFrame()
{
    ImGui::Render();
}

VkCommandBuffer ImGuiUI::recordAndGetCommandBuffer(int currentFrameIndex, int currentImageIndex)
{
    VkCommandBuffer commandBuffer = imguiCommandBuffers[currentFrameIndex];
    vkResetCommandBuffer(commandBuffer, 0);

    VkCommandBufferBeginInfo commandBufferBeginInfo = vulkanInitializers::commandBufferBeginInfo();
    if (vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo) != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to begin recording imgui command buffer!");
    }

    VkRenderPassBeginInfo renderPassBeginInfo{};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = imguiRenderPass;
    renderPassBeginInfo.framebuffer = imguiFramebuffers[currentImageIndex];
    renderPassBeginInfo.renderArea.offset = { 0, 0 };
    renderPassBeginInfo.renderArea.extent = vulkanSwapChain->surfaceExtent;
    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), imguiCommandBuffers[currentFrameIndex]);

    vkCmdEndRenderPass(commandBuffer);
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to record imgui command buffer!");
    }

    return commandBuffer;
}

void ImGuiUI::createDescriptorPool()
{
    const std::vector<VkDescriptorPoolSize> poolSizes =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 20 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 20 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 20 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 20 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 20 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 20 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 20 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 20 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 20 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 20 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 20 }
    };

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = vulkanInitializers::descriptorPoolCreateInfo(poolSizes, 1000);

    if (vkCreateDescriptorPool(vulkanDevice->logicalDevice, &descriptorPoolCreateInfo, nullptr, &imguiDescriptorPool) != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to create imgui descriptor pool!");
    }
}

void ImGuiUI::createCommandPool()
{
    VkCommandPoolCreateInfo commandPoolCreateInfo = vulkanInitializers::commandPoolCreateInfo();
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = vulkanDevice->queueFamilyIndices.graphics.value();

    if (vkCreateCommandPool(vulkanDevice->logicalDevice, &commandPoolCreateInfo, nullptr, &imguiCommandPool) != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to create imgui command pool!");
    }
}

void ImGuiUI::allocateCommandBuffers()
{
    imguiCommandBuffers.resize(maxFramesInFlight);
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = vulkanInitializers::commandBufferAllocateInfo(imguiCommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, maxFramesInFlight);

    if (vkAllocateCommandBuffers(vulkanDevice->logicalDevice, &commandBufferAllocateInfo, imguiCommandBuffers.data()) != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to allocate imgui command buffers!");
    }
}

void ImGuiUI::createRenderPass()
{
    // Attachments descriptions
    std::vector<VkAttachmentDescription> attachmentsDescriptions;
    // attachment description [0] - swap chain image
    attachmentsDescriptions.push_back({ });
    attachmentsDescriptions.back().format = vulkanSwapChain->surfaceFormat.format;
    attachmentsDescriptions.back().samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentsDescriptions.back().loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // Loading the rendered frame
    attachmentsDescriptions.back().storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentsDescriptions.back().stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentsDescriptions.back().stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentsDescriptions.back().initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachmentsDescriptions.back().finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Attachments references
    std::vector<VkAttachmentReference> attachmentsReferences;
    // attachments references [0] - swap chain image
    attachmentsReferences.push_back({ });
    attachmentsReferences.back().attachment = 0;
    attachmentsReferences.back().layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Subpasses
    std::vector<VkSubpassDescription> subpassDescriptions;
    // subbpass [0]
    subpassDescriptions.push_back({ });
    subpassDescriptions.back().pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescriptions.back().colorAttachmentCount = 1;
    subpassDescriptions.back().pColorAttachments = &attachmentsReferences[0];

    // Subpass dependencies
    // Before drawing the UI we have to wait until the main frame is rendered
    std::vector<VkSubpassDependency> subpassDependencies;
    subpassDependencies.push_back({ });
    subpassDependencies.back().srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependencies.back().dstSubpass = 0;
    subpassDependencies.back().srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies.back().srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependencies.back().dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies.back().dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // Create renderpass
    VkRenderPassCreateInfo renderpassCreateInfo = vulkanInitializers::renderPassCreateInfo();
    renderpassCreateInfo.attachmentCount = attachmentsDescriptions.size();
    renderpassCreateInfo.pAttachments = attachmentsDescriptions.data();
    renderpassCreateInfo.subpassCount = subpassDescriptions.size();
    renderpassCreateInfo.pSubpasses = subpassDescriptions.data();
    renderpassCreateInfo.dependencyCount = subpassDependencies.size();
    renderpassCreateInfo.pDependencies = subpassDependencies.data();

    if (vkCreateRenderPass(vulkanDevice->logicalDevice, &renderpassCreateInfo, nullptr, &imguiRenderPass) != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to create imgui renderpass!");
    }
}

void ImGuiUI::createFramebuffers()
{
    imguiFramebuffers.resize(vulkanSwapChain->imagesViews.size());

    for (size_t i = 0; i < vulkanSwapChain->imagesViews.size(); i++) {
        std::vector<VkImageView> attachments = {
            vulkanSwapChain->imagesViews[i]
        };

        VkFramebufferCreateInfo framebufferCreateInfo{};
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.renderPass = imguiRenderPass;
        framebufferCreateInfo.attachmentCount = attachments.size();
        framebufferCreateInfo.pAttachments = attachments.data();
        framebufferCreateInfo.width = vulkanSwapChain->surfaceExtent.width;
        framebufferCreateInfo.height = vulkanSwapChain->surfaceExtent.height;
        framebufferCreateInfo.layers = 1;

        if (vkCreateFramebuffer(vulkanDevice->logicalDevice, &framebufferCreateInfo, nullptr, &imguiFramebuffers[i]) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to create imgui framebuffers!");
        }
    }
}
