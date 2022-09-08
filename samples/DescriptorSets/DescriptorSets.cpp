#include "../../Base/src/BaseSample.h"

std::string EXAMPLE_NAME_STR = std::string("DescriptorSets");

class Sample : public BaseSample
{
    VkDescriptorPool                descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet>    descriptorSets;

    // Input vertexes
    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 color;
    };
    VkBuffer                        vertexesBuffer = VK_NULL_HANDLE;
    VmaAllocation                   vertexesBufferAllocation = 0;
    VmaAllocationInfo               vertexesBufferAllocationInfo{};

    // Matrixes
    struct matrixes
    {
        alignas(16) glm::mat4 projection;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 model;
    };
    std::vector<VkBuffer>           matrixesBuffers;
    std::vector<VmaAllocation>      matrixesBufferAllocations;
    std::vector<VmaAllocationInfo>  matrixesBufferAllocationInfos{};

    VkDescriptorSetLayout           descriptorSetLayout = VK_NULL_HANDLE;

    std::vector<Vertex> vertexes = {
        { glm::vec3(0.0, -0.5, 0.0), glm::vec3(1.0, 0.0, 0.0) },
        { glm::vec3(0.5, 0.5, 0.0), glm::vec3(0.0, 1.0, 0.0) },
        { glm::vec3(-0.5, 0.5, 0.0), glm::vec3(0.0, 0.0, 1.0) }
    };

    VkShaderModule      vertShaderModule = VK_NULL_HANDLE;
    VkShaderModule      fragShaderModule = VK_NULL_HANDLE;
    VkPipelineLayout    pipelineLayout = VK_NULL_HANDLE;
    VkPipeline          graphicsPipeline = VK_NULL_HANDLE;

public:
    Sample()
    {
        // Setting sample requirements
        base_title = "Descriptor sets";
    }

    void prepare()
    {
        BaseSample::prepare();
        initCamera();
        createBuffers();
        createDescriptorSetLayouts();
        createDescriptorSets();
        createGraphicsPipeline();
    }

    void cleanup()
    {
        vkDeviceWaitIdle(base_vulkanDevice->logicalDevice);

        // Pipeline
        vkDestroyShaderModule(base_vulkanDevice->logicalDevice, vertShaderModule, nullptr);
        vkDestroyShaderModule(base_vulkanDevice->logicalDevice, fragShaderModule, nullptr);
        vkDestroyPipelineLayout(base_vulkanDevice->logicalDevice, pipelineLayout, nullptr);
        vkDestroyPipeline(base_vulkanDevice->logicalDevice, graphicsPipeline, nullptr);

        // Descriptor pool and allocated sets
        vkDestroyDescriptorPool(base_vulkanDevice->logicalDevice, descriptorPool, nullptr);

        // Descriptor set layouts
        vkDestroyDescriptorSetLayout(base_vulkanDevice->logicalDevice, descriptorSetLayout, nullptr);

        // Buffers
        for (uint32_t i = 0; i < matrixesBuffers.size(); ++i) {
            vmaDestroyBuffer(base_vmaAllocator, matrixesBuffers[i], matrixesBufferAllocations[i]);
        }
        vmaDestroyBuffer(base_vmaAllocator, vertexesBuffer, vertexesBufferAllocation);
    }

    ~Sample()
    {
    }

    void draw()
    {
        // Acquire image
        BaseSample::prepareFrame();

        updateUniformBuffer(base_currentFrameIndex);

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
        base_camera.setPerspective(45.0f, (float)base_windowWidth / (float)base_windowHeight, 0.1f, 10.0f);
        base_camera.setTranslation(glm::vec3(0.0f, 0.0f, -2.0f));
    }

    void createBuffers()
    {
        // Vertex buffer
        VkBufferCreateInfo vertexBufferCreateInfo{};
        vertexBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        vertexBufferCreateInfo.size = VkDeviceSize(sizeof(vertexes[0])) * vertexes.size();
        vertexBufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        vertexBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo vertexBufferAllocationCreateInfo{};
        vertexBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        vertexBufferAllocationCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        vertexBufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT; // We must use memcpy (not random access!)

        if (vmaCreateBuffer(base_vmaAllocator, &vertexBufferCreateInfo, &vertexBufferAllocationCreateInfo, &vertexesBuffer, &vertexesBufferAllocation, &vertexesBufferAllocationInfo) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to create buffer!");
        }

        // Map buffer memory and copy vertexes data
        void* mappedBufferData = nullptr;
        if (vmaMapMemory(base_vmaAllocator, vertexesBufferAllocation, &mappedBufferData) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to map buffer!");
        }
        memcpy(mappedBufferData, vertexes.data(), (size_t)vertexBufferCreateInfo.size);
        vmaUnmapMemory(base_vmaAllocator, vertexesBufferAllocation);

        // matrixes buffer
        VkBufferCreateInfo matrixesBufferCreateInfo{};
        matrixesBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        matrixesBufferCreateInfo.size = VkDeviceSize(sizeof(matrixes));
        matrixesBufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        matrixesBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocationCreateInfo{};
        allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocationCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        allocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT; // We must use memcpy (not random access!)
        
        matrixesBuffers.resize(BASE_MAX_FRAMES_IN_FLIGHT);
        matrixesBufferAllocations.resize(BASE_MAX_FRAMES_IN_FLIGHT);
        matrixesBufferAllocationInfos.resize(BASE_MAX_FRAMES_IN_FLIGHT);
        for (uint32_t i = 0; i < matrixesBuffers.size(); ++i) {
            if (vmaCreateBuffer(base_vmaAllocator, &matrixesBufferCreateInfo, &allocationCreateInfo, &matrixesBuffers[i], &matrixesBufferAllocations[i], &matrixesBufferAllocationInfos[i]) != VK_SUCCESS) {
                throw MakeErrorInfo("Failed to create buffer!");
            }
        }
    }

    void createDescriptorSetLayouts()
    {
        // Binding for descriptor set
        VkDescriptorSetLayoutBinding descriptorSetLayoutBinding{};
        descriptorSetLayoutBinding.binding = 0;
        descriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorSetLayoutBinding.descriptorCount = 1;
        descriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
        descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCreateInfo.bindingCount = 1;
        descriptorSetLayoutCreateInfo.pBindings = &descriptorSetLayoutBinding;

        if (vkCreateDescriptorSetLayout(base_vulkanDevice->logicalDevice, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to create descriptor set!");
        }
    }

    void createDescriptorSets()
    {
        // We need several descriptors to bind the buffer(one per frame)
        VkDescriptorPoolSize descriptorPoolSize{};
        descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorPoolSize.descriptorCount = BASE_MAX_FRAMES_IN_FLIGHT;

        VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
        descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolCreateInfo.poolSizeCount = 1;
        descriptorPoolCreateInfo.pPoolSizes = &descriptorPoolSize;
        // Even one descriptor is contained in a set, so we need several sets of descriptors(one per frame)
        descriptorPoolCreateInfo.maxSets = BASE_MAX_FRAMES_IN_FLIGHT;

        if (vkCreateDescriptorPool(base_vulkanDevice->logicalDevice, &descriptorPoolCreateInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to create descriptor pool!");
        }

        // Allocate descriptor sets(one descriptor set per frame)
        descriptorSets.resize(BASE_MAX_FRAMES_IN_FLIGHT);
        std::vector<VkDescriptorSetLayout> setsLayouts(BASE_MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
        VkDescriptorSetAllocateInfo setsAllocInfo{};
        setsAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        setsAllocInfo.descriptorPool = descriptorPool;
        setsAllocInfo.descriptorSetCount = BASE_MAX_FRAMES_IN_FLIGHT;
        setsAllocInfo.pSetLayouts = setsLayouts.data();

        if (vkAllocateDescriptorSets(base_vulkanDevice->logicalDevice, &setsAllocInfo, descriptorSets.data()) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to allocate descriptor sets!");
        }

        // Write descriptors
        for (size_t i = 0; i < BASE_MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo bufferDescriptorInfo{};
            bufferDescriptorInfo.buffer = matrixesBuffers[i];
            bufferDescriptorInfo.offset = 0;
            bufferDescriptorInfo.range = sizeof(matrixes);

            VkWriteDescriptorSet writeDescriptorSet{};
            writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptorSet.dstSet = descriptorSets[i];
            writeDescriptorSet.dstBinding = 0;
            writeDescriptorSet.dstArrayElement = 0;
            writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writeDescriptorSet.descriptorCount = 1;
            writeDescriptorSet.pBufferInfo = &bufferDescriptorInfo;

            vkUpdateDescriptorSets(base_vulkanDevice->logicalDevice, 1, &writeDescriptorSet, 0, nullptr);
        }
    }

    void createGraphicsPipeline()
    {
        vertShaderModule = vulkanTools::loadShader(base_vulkanDevice->logicalDevice, ASSETS_DATA_SHADERS_PATH + EXAMPLE_NAME_STR + "/vertshader.vert.spv");
        fragShaderModule = vulkanTools::loadShader(base_vulkanDevice->logicalDevice, ASSETS_DATA_SHADERS_PATH + EXAMPLE_NAME_STR + "/fragshader.frag.spv");

        std::vector<VkPipelineShaderStageCreateInfo> shaderStagesCreateInfos = {
            vulkanInitializers::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertShaderModule),
            vulkanInitializers::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragShaderModule)
        };

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

        // Color
        attributeDescriptions.push_back({});
        attributeDescriptions.back().location = 1;
        attributeDescriptions.back().binding = 0;
        attributeDescriptions.back().format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions.back().offset = offsetof(Vertex, color);

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
        viewport.y = 0.0f;
        viewport.width = (float)base_vulkanSwapChain->surfaceExtent.width;
        viewport.height = (float)base_vulkanSwapChain->surfaceExtent.height;
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
        rasterizerInfo.cullMode = VK_CULL_MODE_BACK_BIT;
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
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
        pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

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

        if (vkCreateGraphicsPipelines(base_vulkanDevice->logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to create graphics pipeline!");
        }
    }

    void updateUniformBuffer(uint32_t currentFrame)
    {
        static double timeElapsed = 0;
        timeElapsed += base_frameTime;
        static float rotation = 0;
        matrixes matrix{};
        matrix.model = glm::rotate(glm::mat4(1.0f), glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f));

        base_camera.updateAspectRatio((float)base_windowWidth / (float)base_windowHeight);
        base_camera.update((float)base_frameTime / 1000.0f);
        matrix.projection = base_camera.matrices.perspective;
        matrix.view = base_camera.matrices.view;

        void* mappedBufferData = nullptr;
        if (vmaMapMemory(base_vmaAllocator, matrixesBufferAllocations[currentFrame], &mappedBufferData)) {
            throw MakeErrorInfo("Failed to map buffer!");
        }
        memcpy(mappedBufferData, &matrix, sizeof(matrixes));
        vmaUnmapMemory(base_vmaAllocator, matrixesBufferAllocations[currentFrame]);
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

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(base_vulkanSwapChain->surfaceExtent.width);
        viewport.height = static_cast<float>(base_vulkanSwapChain->surfaceExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = base_vulkanSwapChain->surfaceExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        std::vector<VkDeviceSize> offsets(vertexes.size(), 0);
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexesBuffer, offsets.data());

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[base_currentFrameIndex], 0, nullptr);

        vkCmdDraw(commandBuffer, 3, 1, 0, 0);

        vkCmdEndRenderPass(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to record command buffer!");
        }
    }

    void drawUI()
    {
        
    }
};

EXAMPLE_MAIN(Sample)
