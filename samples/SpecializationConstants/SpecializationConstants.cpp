#include "../../Base/BaseSample.h"

/*
 *    Using specialization constants to set the constants in the shader during the creation of the pipelines
 */

std::string EXAMPLE_NAME_STR = std::string("SpecializationConstants");

class Sample : public BaseSample
{
    // Input vertexes
    struct Vertex
    {
        glm::vec3 position;
    };
    VulkanBuffer            vertexBuffer;

    // Matrixes
    struct MatrixPushConstant
    {
        alignas(16) glm::mat4 MVPmatrix;
    };
    MatrixPushConstant matrixPushConstant;

    struct ColorsSpecializationData
    {
        // Must be scalar types
        float vertexColorR; // constant_id 0
        float vertexColorG; // constant_id 1
        float vertexColorB; // constant_id 2
    };

    VkPipeline cyanGraphicsPipeline = VK_NULL_HANDLE;
    VkPipeline pinkGraphicsPipeline = VK_NULL_HANDLE;


    std::vector<Vertex> vertexes = {
        { glm::vec3(0.0, 0.5, 0.0) },
        { glm::vec3(0.5, -0.5, 0.0) },
        { glm::vec3(-0.5, -0.5, 0.0) }
    };

    VkShaderModule      vertShaderModule = VK_NULL_HANDLE;
    VkShaderModule      fragShaderModule = VK_NULL_HANDLE;
    VkPipelineLayout    pipelineLayout = VK_NULL_HANDLE;

public:
    Sample()
    {
        // Setting sample requirements
        base_title = "Specialization Constants";
    }

    void prepare()
    {
        BaseSample::prepare();
        initCamera();
        createBuffers();
        createGraphicsPipelines();
    }

    void cleanup()
    {
        vkDeviceWaitIdle(base_vulkanDevice->logicalDevice);

        // Pipeline
        vkDestroyShaderModule(base_vulkanDevice->logicalDevice, vertShaderModule, nullptr);
        vkDestroyShaderModule(base_vulkanDevice->logicalDevice, fragShaderModule, nullptr);
        vkDestroyPipelineLayout(base_vulkanDevice->logicalDevice, pipelineLayout, nullptr);
        vkDestroyPipeline(base_vulkanDevice->logicalDevice, cyanGraphicsPipeline, nullptr);
        vkDestroyPipeline(base_vulkanDevice->logicalDevice, pinkGraphicsPipeline, nullptr);

        // Buffers
        vertexBuffer.destroy();
    }

    void draw()
    {
        // Acquire image
        BaseSample::prepareFrame();

        updatePushConstants();

        // Record command buffer
        vkResetCommandBuffer(base_commandBuffersGraphics[base_currentFrameIndex], 0);
        recordCommandBuffer(base_commandBuffersGraphics[base_currentFrameIndex], base_currentImageIndex);

        // Draw UI
        imguiUI.beginFrame();
        drawUI();
        imguiUI.endFrame();

        VkCommandBuffer imguiCommandBuffer;
        imguiCommandBuffer = imguiUI.recordAndGetCommandBuffer(base_currentFrameIndex, base_currentImageIndex);

        std::vector<VkCommandBuffer> submittableCommandBuffer{ base_commandBuffersGraphics[base_currentFrameIndex], imguiCommandBuffer };

        base_submitInfo.commandBufferCount = submittableCommandBuffer.size();
        base_submitInfo.pCommandBuffers = submittableCommandBuffer.data();
        setupSubmitInfo(base_currentFrameIndex);
        if (vkQueueSubmit(base_graphicsQueue, 1, &base_submitInfo, base_inFlightFences[base_currentFrameIndex]) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to submit command buffers!");
        }

        // Present image
        BaseSample::submitFrame();
    }

    bool getEnabledFeatures(VkPhysicalDevice physicalDevice)
    {
        return true;
    }

    void initCamera()
    {
        // Setting up camera
        base_camera.type = Camera::CameraType::firstperson;
        base_camera.setPerspective(45.0f, ((float)base_windowWidth / 2.0f) / (float)base_windowHeight, 0.1f, 100.0f);
        base_camera.setPosition(glm::vec3(0.0f, 0.0f, -6.0f));
    }

    void createBuffers()
    {
        // Vertex buffer
        vertexBuffer.setDeviceAndAllocator(base_vulkanDevice, base_vmaAllocator);
        vertexBuffer.createBuffer(
            VkDeviceSize(sizeof(vertexes[0])) * vertexes.size(),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            vertexes.data(),
            VkDeviceSize(sizeof(vertexes[0])) * vertexes.size()
        );
    }

    void createGraphicsPipelines()
    {
        vertShaderModule = vulkanTools::loadShader(base_vulkanDevice->logicalDevice, ASSETS_DATA_SHADERS_PATH + EXAMPLE_NAME_STR + "/vertshader.vert.spv");
        fragShaderModule = vulkanTools::loadShader(base_vulkanDevice->logicalDevice, ASSETS_DATA_SHADERS_PATH + EXAMPLE_NAME_STR + "/fragshader.frag.spv");

        // Cyan triangle
        ColorsSpecializationData cyanSpecializationData{};
        cyanSpecializationData.vertexColorR = 0.0f;
        cyanSpecializationData.vertexColorG = 1.0f;
        cyanSpecializationData.vertexColorB = 1.0f;

        // Each shader constant of a shader stage corresponds to one map entry
        // Shader bindings based on specialization constants are marked by the new "constant_id" layout qualifier:
        // layout (constant_id = 0) const vec4 vertexColor;
        std::vector<VkSpecializationMapEntry> specializationMapEntries{};
        specializationMapEntries.push_back({ });
        specializationMapEntries.back().constantID = 0;
        specializationMapEntries.back().offset = 0;
        specializationMapEntries.back().size = sizeof(ColorsSpecializationData::vertexColorR); // float

        specializationMapEntries.push_back({ });
        specializationMapEntries.back().constantID = 1;
        specializationMapEntries.back().offset = sizeof(ColorsSpecializationData::vertexColorR) * 1;
        specializationMapEntries.back().size = sizeof(ColorsSpecializationData::vertexColorG); // float

        specializationMapEntries.push_back({ });
        specializationMapEntries.back().constantID = 2;
        specializationMapEntries.back().offset = sizeof(ColorsSpecializationData::vertexColorR) * 2;
        specializationMapEntries.back().size = sizeof(ColorsSpecializationData::vertexColorB); // float

        VkSpecializationInfo specializationInfo{};
        specializationInfo.mapEntryCount = 3;
        specializationInfo.pMapEntries = specializationMapEntries.data();
        specializationInfo.dataSize = sizeof(ColorsSpecializationData);
        specializationInfo.pData = &cyanSpecializationData;

        std::vector<VkPipelineShaderStageCreateInfo> shaderStagesCreateInfos = {
            vulkanInitializers::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertShaderModule),
            vulkanInitializers::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragShaderModule)
        };

        // Fragment shader specialization info
        shaderStagesCreateInfos[1].pSpecializationInfo = &specializationInfo;

        // Dynamic states
        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
        dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicStateInfo.dynamicStateCount = dynamicStates.size();
        dynamicStateInfo.pDynamicStates = dynamicStates.data();

        // Vertex input
        // Binding description for buffer
        VkVertexInputBindingDescription bindingDescriptionInfo{};
        bindingDescriptionInfo.binding = 0;
        bindingDescriptionInfo.stride = sizeof(vertexes[0]);
        bindingDescriptionInfo.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
        // Position
        attributeDescriptions.push_back({});
        attributeDescriptions.back().location = 0;
        attributeDescriptions.back().binding = 0;
        attributeDescriptions.back().format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions.back().offset = offsetof(Vertex, position);

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescriptionInfo;
        vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptions.size();
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        // Input assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
        inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

        // Viewport and scissor
        // Viewport
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = static_cast<float>(base_vulkanSwapChain->surfaceExtent.height);;
        viewport.width = static_cast<float>(base_vulkanSwapChain->surfaceExtent.width);
        viewport.height = -static_cast<float>(base_vulkanSwapChain->surfaceExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        // Scissor
        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = base_vulkanSwapChain->surfaceExtent;
        // Viewport and scissor state
        VkPipelineViewportStateCreateInfo viewportAndScissorInfo{};
        viewportAndScissorInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportAndScissorInfo.viewportCount = 1;
        viewportAndScissorInfo.pViewports = &viewport;
        viewportAndScissorInfo.scissorCount = 1;
        viewportAndScissorInfo.pScissors = &scissor;

        // Rasterizer
        VkPipelineRasterizationStateCreateInfo rasterizerInfo{};
        rasterizerInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizerInfo.depthClampEnable = VK_FALSE;
        rasterizerInfo.rasterizerDiscardEnable = VK_FALSE;
        rasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizerInfo.lineWidth = 1.0f;
        rasterizerInfo.cullMode = VK_CULL_MODE_NONE;
        rasterizerInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizerInfo.depthBiasEnable = VK_FALSE;
        rasterizerInfo.depthBiasConstantFactor = 0.0f; // Optional
        rasterizerInfo.depthBiasClamp = 0.0f; // Optional
        rasterizerInfo.depthBiasSlopeFactor = 0.0f; // Optional

        // Multisampling
        VkPipelineMultisampleStateCreateInfo multisamplingInfo{};
        multisamplingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisamplingInfo.sampleShadingEnable = VK_FALSE;
        multisamplingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisamplingInfo.minSampleShading = 1.0f; // Optional
        multisamplingInfo.pSampleMask = nullptr; // Optional
        multisamplingInfo.alphaToCoverageEnable = VK_FALSE; // Optional
        multisamplingInfo.alphaToOneEnable = VK_FALSE; // Optional

        // Depth stencil state 
        VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
        depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencilInfo.depthTestEnable = VK_TRUE;
        depthStencilInfo.depthWriteEnable = VK_TRUE;
        depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
        depthStencilInfo.minDepthBounds = 0.0f; // Optional
        depthStencilInfo.maxDepthBounds = 1.0f; // Optional
        depthStencilInfo.stencilTestEnable = VK_FALSE;
        depthStencilInfo.front = {}; // Optional
        depthStencilInfo.back = {}; // Optional

        // Color blending
        VkPipelineColorBlendAttachmentState colorBlendAttachmentInfo{};
        colorBlendAttachmentInfo.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachmentInfo.blendEnable = VK_FALSE;
        colorBlendAttachmentInfo.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachmentInfo.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachmentInfo.colorBlendOp = VK_BLEND_OP_ADD; // Optional
        colorBlendAttachmentInfo.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachmentInfo.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachmentInfo.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

        VkPipelineColorBlendStateCreateInfo colorBlendingInfo{};
        colorBlendingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendingInfo.logicOpEnable = VK_FALSE;
        colorBlendingInfo.logicOp = VK_LOGIC_OP_COPY; // Optional
        colorBlendingInfo.attachmentCount = 1;
        colorBlendingInfo.pAttachments = &colorBlendAttachmentInfo;
        colorBlendingInfo.blendConstants[0] = 0.0f; // Optional
        colorBlendingInfo.blendConstants[1] = 0.0f; // Optional
        colorBlendingInfo.blendConstants[2] = 0.0f; // Optional
        colorBlendingInfo.blendConstants[3] = 0.0f; // Optional

        // Pipeline layout
        // Specifying a push constant range with matrix for vertex shader
        VkPushConstantRange vertexMatrixPushConstantRange{};
        vertexMatrixPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        // offset and size must be a multiple of 4
        // Spec says: size must be less than or equal to VkPhysicalDeviceLimits::maxPushConstantsSize minus offset
        vertexMatrixPushConstantRange.offset = 0;
        vertexMatrixPushConstantRange.size = sizeof(MatrixPushConstant);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0;
        pipelineLayoutInfo.pSetLayouts = nullptr;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &vertexMatrixPushConstantRange;

        if (vkCreatePipelineLayout(base_vulkanDevice->logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to create pipeline layout!");
        }

        // Create graphics pipeline
        VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
        pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineCreateInfo.stageCount = shaderStagesCreateInfos.size();
        pipelineCreateInfo.pStages = shaderStagesCreateInfos.data();
        pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
        pipelineCreateInfo.pInputAssemblyState = &inputAssemblyInfo;
        pipelineCreateInfo.pViewportState = &viewportAndScissorInfo;
        pipelineCreateInfo.pRasterizationState = &rasterizerInfo;
        pipelineCreateInfo.pMultisampleState = &multisamplingInfo;
        pipelineCreateInfo.pDepthStencilState = &depthStencilInfo;
        pipelineCreateInfo.pColorBlendState = &colorBlendingInfo;
        pipelineCreateInfo.pDynamicState = &dynamicStateInfo;
        pipelineCreateInfo.layout = pipelineLayout;
        pipelineCreateInfo.renderPass = base_renderPass;
        pipelineCreateInfo.subpass = 0;

        // Create graphics pipeline with cyan specialization data for cyan triangle
        if (vkCreateGraphicsPipelines(base_vulkanDevice->logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &cyanGraphicsPipeline) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to create graphics pipeline!");
        }

        // Pink triangle
        ColorsSpecializationData pinkSpecializationData{};
        pinkSpecializationData.vertexColorR = 1.0f;
        pinkSpecializationData.vertexColorG = 0.0f;
        pinkSpecializationData.vertexColorB = 0.5f;

        // Change specialization data for pink triangle
        specializationInfo.dataSize = sizeof(ColorsSpecializationData);
        specializationInfo.pData = &pinkSpecializationData;

        // Create graphics pipeline with pink specialization data for pink triangle
        if (vkCreateGraphicsPipelines(base_vulkanDevice->logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pinkGraphicsPipeline) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to create graphics pipeline!");
        }
    }

    void updatePushConstants()
    {
        static double timeElapsed = 0;
        timeElapsed += base_frameTime;
        static float rotation = 0;

        base_camera.updateAspectRatio(((float)base_windowWidth / 2.0f) / (float)base_windowHeight);
        base_camera.update((float)base_frameTime / 1000.0f);

        // translate rotate scale
        rotation += 45.0f * (float)base_frameTime / 1000.f;

        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::mat4(1.0f);
        transform = glm::translate(transform, glm::vec3(0.0f, 0.0f, 0.0f));
        transform = glm::rotate(transform, glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f));
        transform = glm::scale(transform, glm::vec3(2.0f, 2.0f, 1.0f));
        matrixPushConstant.MVPmatrix = base_camera.matrices.perspective * base_camera.matrices.view * transform;
    }

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
    {
        VkCommandBufferBeginInfo commandBufferBeginInfo{};
        commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassBeginInfo{};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.renderPass = base_renderPass;
        renderPassBeginInfo.framebuffer = base_swapChainFramebuffers[imageIndex];
        renderPassBeginInfo.renderArea.offset = { 0, 0 };
        renderPassBeginInfo.renderArea.extent = base_vulkanSwapChain->surfaceExtent;
        std::vector<VkClearValue> clearValues(2);
        clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
        clearValues[1].depthStencil = { 1.0f, 0 };
        renderPassBeginInfo.clearValueCount = clearValues.size();
        renderPassBeginInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        std::vector<VkDeviceSize> offsets(vertexes.size(), 0);
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer.buffer, offsets.data());

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = static_cast<float>(base_vulkanSwapChain->surfaceExtent.height);
        viewport.width = static_cast<float>(base_vulkanSwapChain->surfaceExtent.width) / 2.0f;
        viewport.height = -static_cast<float>(base_vulkanSwapChain->surfaceExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = base_vulkanSwapChain->surfaceExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MatrixPushConstant), &matrixPushConstant);
        
        // Cyan triangle
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, cyanGraphicsPipeline);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);

        // Pink triangle
        viewport.x = viewport.width;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pinkGraphicsPipeline);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);

        vkCmdEndRenderPass(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to record command buffer!");
        }
    }

    void drawUI()
    {
        UIOverlay::windowBegin(base_title.c_str(), nullptr, { 0, 0 }, { 200, 50 });
        UIOverlay::printFPS((float)base_frameTime, 500);
        UIOverlay::windowEnd();
    }
};

EXAMPLE_MAIN(Sample)
