#include "../../Base/src/BaseSample.h"

std::string EXAMPLE_NAME_STR = std::string("glTFloading");

class Sample : public BaseSample
{
    vulkanglTF::Model*      model = nullptr;

    struct ShaderData {
        VulkanBuffer* vulkanBuffer = nullptr;
        struct Values {
            glm::mat4 projection = glm::mat4(1.0f);
            glm::mat4 view = glm::mat4(1.0f);
            glm::mat4 model_transform = glm::mat4(1.0f);
        } values;
    };

    // One per frame
    std::vector<ShaderData> shaderData;

    VkShaderModule          vertShaderModule = VK_NULL_HANDLE;
    VkShaderModule          fragShaderModule = VK_NULL_HANDLE;
    VkPipelineLayout        pipelineLayout = VK_NULL_HANDLE;
    VkPipeline              graphicsPipeline = VK_NULL_HANDLE;

    struct DescriptorSetLayouts {
        VkDescriptorSetLayout matrices;
        VkDescriptorSetLayout textures;
    } descriptorSetLayouts;
    VkDescriptorPool        descriptorPool = VK_NULL_HANDLE;
    // One per frame
    std::vector<VkDescriptorSet> buffersDescriptorSets;
public:
    Sample()
    {
        base_title = "glTF loading";
    }

    void prepare()
    {
        BaseSample::prepare();
        initCamera();
        loadAssets();
        prepareUniformBuffers();
        setupDescriptors();
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

        // Descriptor pool
        vkDestroyDescriptorPool(base_vulkanDevice->logicalDevice, descriptorPool, nullptr);

        // Descriptor set layouts
        vkDestroyDescriptorSetLayout(base_vulkanDevice->logicalDevice, descriptorSetLayouts.matrices, nullptr);
        vkDestroyDescriptorSetLayout(base_vulkanDevice->logicalDevice, descriptorSetLayouts.textures, nullptr);

        // Uniform buffer
        for (uint32_t i = 0; i < BASE_MAX_FRAMES_IN_FLIGHT; ++i) {
            shaderData[i].vulkanBuffer->destroy();
            delete shaderData[i].vulkanBuffer;
        }

        // Model
        delete model;
    }

    ~Sample()
    {
    }

    void draw()
    {
        // Acquire image
        BaseSample::prepareFrame();

        updateUniformBuffers(base_currentFrameIndex);

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
        base_camera.type = Camera::CameraType::lookat;
        base_camera.flipY = true;
        base_camera.setPerspective(60.0f, (float)base_windowWidth / (float)base_windowHeight, 0.1f, 100.0f);
        base_camera.setTranslation(glm::vec3(0.0f, -0.1f, -1.0f));
        base_camera.setRotation(glm::vec3(0.0f, -135.0f, 0.0f));
    }

    void loadAssets()
    {
        model = new vulkanglTF::Model(base_vulkanDevice, base_graphicsQueue, base_commandPoolGraphics, base_vmaAllocator);
        model->loadFromFile(ASSETS_DATA_PATH + "/models/FlightHelmet/glTF/FlightHelmet.gltf", base_graphicsQueue, base_commandPoolGraphics);
    }

    void prepareUniformBuffers()
    {
        shaderData.resize(BASE_MAX_FRAMES_IN_FLIGHT);
        for (uint32_t i = 0; i < BASE_MAX_FRAMES_IN_FLIGHT; ++i) {
            shaderData[i].vulkanBuffer = new VulkanBuffer(base_vulkanDevice, base_vmaAllocator);
            shaderData[i].vulkanBuffer->createBuffer(
                sizeof(ShaderData),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
            );
        }
    }

    void setupDescriptors()
    {
        // Create descriptor pool
        std::vector<VkDescriptorPoolSize> poolSizes = {
            vulkanInitializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, BASE_MAX_FRAMES_IN_FLIGHT),
            // One combined image sampler per model image/texture
            vulkanInitializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, model->images.size())
        };
        uint32_t maxSetsCount = model->images.size() + BASE_MAX_FRAMES_IN_FLIGHT;
        VkDescriptorPoolCreateInfo descriptorPoolInfo = vulkanInitializers::descriptorPoolCreateInfo(poolSizes, maxSetsCount);
        if (vkCreateDescriptorPool(base_vulkanDevice->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to create descriptor pool!");
        }

        // Create descriptor set layouts
        // Descriptor set layout for passing matrices using Uniform Buffer
        VkDescriptorSetLayoutBinding setLayoutBinding{};
        setLayoutBinding.binding = 0;
        setLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        setLayoutBinding.descriptorCount = 1;
        setLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
        descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCreateInfo.bindingCount = 1;
        descriptorSetLayoutCreateInfo.pBindings = &setLayoutBinding;
        if (vkCreateDescriptorSetLayout(base_vulkanDevice->logicalDevice, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayouts.matrices) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to create descriptor set layout")
        }

        // Descriptor set layout for passing textures
        setLayoutBinding.binding = 0;
        setLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        setLayoutBinding.descriptorCount = 1;
        setLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        
        descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCreateInfo.bindingCount = 1;
        descriptorSetLayoutCreateInfo.pBindings = &setLayoutBinding;
        if (vkCreateDescriptorSetLayout(base_vulkanDevice->logicalDevice, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayouts.textures) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to create descriptor set layout")
        }

        // Allocate and write descriptor sets
        // Allocate multiple descriptor sets for matrices buffer(one per frame)
        VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
        descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorSetAllocateInfo.descriptorPool = descriptorPool;
        descriptorSetAllocateInfo.descriptorSetCount = BASE_MAX_FRAMES_IN_FLIGHT;
        std::vector<VkDescriptorSetLayout> descriptorSetsLayouts(BASE_MAX_FRAMES_IN_FLIGHT, descriptorSetLayouts.matrices);
        descriptorSetAllocateInfo.pSetLayouts = descriptorSetsLayouts.data();
        buffersDescriptorSets.resize(BASE_MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(base_vulkanDevice->logicalDevice, &descriptorSetAllocateInfo, buffersDescriptorSets.data()) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to allocate descriptor sets!");
        }
        // Write buffers into descriptors
        for (size_t i = 0; i < BASE_MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo bufferDescriptorInfo{};
            bufferDescriptorInfo.buffer = shaderData[i].vulkanBuffer->buffer;
            bufferDescriptorInfo.offset = 0;
            bufferDescriptorInfo.range = sizeof(ShaderData::Values);

            VkWriteDescriptorSet writeDescriptorSet{};
            writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptorSet.dstSet = buffersDescriptorSets[i];
            writeDescriptorSet.dstBinding = 0;
            writeDescriptorSet.dstArrayElement = 0;
            writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writeDescriptorSet.descriptorCount = 1;
            writeDescriptorSet.pBufferInfo = &bufferDescriptorInfo;

            vkUpdateDescriptorSets(base_vulkanDevice->logicalDevice, 1, &writeDescriptorSet, 0, nullptr);
        }

        // Allocate descriptor set for images(textures)
        for (auto& image : model->images) {
            // Allocate one descriptor set per image(texture)
            VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
            descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            descriptorSetAllocateInfo.descriptorPool = descriptorPool;
            descriptorSetAllocateInfo.descriptorSetCount = 1;
            descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayouts.textures;
            if (vkAllocateDescriptorSets(base_vulkanDevice->logicalDevice, &descriptorSetAllocateInfo, &image.descriptorSet) != VK_SUCCESS) {
                throw MakeErrorInfo("Failed to allocate descriptor sets!");
            }
            // Write image(texture) into descriptor 
            VkWriteDescriptorSet writeDescriptorSet{};
            writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptorSet.dstSet = image.descriptorSet;
            writeDescriptorSet.dstBinding = 0;
            writeDescriptorSet.dstArrayElement = 0;
            writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writeDescriptorSet.descriptorCount = 1;
            writeDescriptorSet.pImageInfo = &image.texture.descriptor;

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

        VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = vulkanInitializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
        VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = vulkanInitializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
        VkPipelineColorBlendAttachmentState blendAttachmentStateCreateInfo = vulkanInitializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
        VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = vulkanInitializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentStateCreateInfo);
        VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = vulkanInitializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
        VkPipelineViewportStateCreateInfo viewportStateCreateInfo = vulkanInitializers::pipelineViewportStateCreateInfo(1, 1, 0);
        VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = vulkanInitializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
        const std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = vulkanInitializers::pipelineDynamicStateCreateInfo(dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);
        // Vertex input bindings and attributes
        const std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
            vulkanInitializers::vertexInputBindingDescription(0, sizeof(vulkanglTF::Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
        };
        const std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
            vulkanInitializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vulkanglTF::Vertex, pos)),	// Location 0: Position
            vulkanInitializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vulkanglTF::Vertex, normal)),// Location 1: Normal
            vulkanInitializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vulkanglTF::Vertex, uv)),	// Location 2: Texture coordinates
            vulkanInitializers::vertexInputAttributeDescription(0, 3, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vulkanglTF::Vertex, color)),	// Location 3: Color
        };

        VkPipelineVertexInputStateCreateInfo* vertexInputStateCreateInfo = vulkanglTF::Vertex::getPipelineVertexInputState({
            vulkanglTF::VertexComponent::Position,
            vulkanglTF::VertexComponent::Normal,
            vulkanglTF::VertexComponent::UV,
            vulkanglTF::VertexComponent::Color
        });

        // Pipeline layout
        VkPushConstantRange vertexMatrixPushConstantRange{};
        vertexMatrixPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        vertexMatrixPushConstantRange.offset = 0;
        vertexMatrixPushConstantRange.size = sizeof(glm::mat4);

        std::vector<VkDescriptorSetLayout> descriptorSetLayoutsList = { descriptorSetLayouts.matrices, descriptorSetLayouts.textures };
        std::vector<VkPushConstantRange> pushConstantsRanges{ vertexMatrixPushConstantRange };
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = descriptorSetLayoutsList.size();
        pipelineLayoutInfo.pSetLayouts = descriptorSetLayoutsList.data();
        pipelineLayoutInfo.pushConstantRangeCount = pushConstantsRanges.size();
        pipelineLayoutInfo.pPushConstantRanges = pushConstantsRanges.data();

        if (vkCreatePipelineLayout(base_vulkanDevice->logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to create pipeline layout!");
        }

        // Create graphics pipeline
        VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
        pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineCreateInfo.stageCount = shaderStagesCreateInfos.size();
        pipelineCreateInfo.pStages = shaderStagesCreateInfos.data();
        pipelineCreateInfo.pVertexInputState = vertexInputStateCreateInfo;
        pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
        pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
        pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
        pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
        pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
        pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
        pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
        pipelineCreateInfo.layout = pipelineLayout;
        pipelineCreateInfo.renderPass = base_renderPass;
        pipelineCreateInfo.subpass = 0;

        if (vkCreateGraphicsPipelines(base_vulkanDevice->logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to create graphics pipeline!");
        }
    }

    void updateUniformBuffers(uint32_t currentFrame)
    {
        base_camera.updateAspectRatio((float)base_windowWidth / (float)base_windowHeight);
        base_camera.update((float)base_frameTime / 1000.0f);

        glm::mat4 model_transform = glm::mat4(1.0f);
        shaderData[currentFrame].values.projection = base_camera.matrices.perspective;
        shaderData[currentFrame].values.view = base_camera.matrices.view;
        shaderData[currentFrame].values.model_transform = model_transform;

        //glm::vec3 cameraPos = base_camera.getPosition();
        //std::cout << cameraPos.x << " " << cameraPos.y << " " << cameraPos.z << "\n";

        void* pMappedBuffer = nullptr;
        shaderData[currentFrame].vulkanBuffer->map(&pMappedBuffer);
        memcpy(pMappedBuffer, &shaderData[currentFrame].values, sizeof(ShaderData::Values));
        shaderData[currentFrame].vulkanBuffer->unmap();
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
        clearValues[0].color = { { 0.25f, 0.25f, 0.25f, 1.0f } };
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

        // Bind scene matrices descriptor to set 0
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &buffersDescriptorSets[base_currentFrameIndex], 0, nullptr);
        model->bindBuffers(commandBuffer);
        model->draw(commandBuffer, pipelineLayout);

        vkCmdEndRenderPass(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to record command buffer!");
        }
    }

    void drawUI()
    {
        //UIOverlay::windowBegin(base_title.c_str(), nullptr, { 0, 0 }, { 300, 100 });
        //UIOverlay::printFPS((float)base_frameTime, 500);
        //UIOverlay::windowEnd();
    }
};

EXAMPLE_MAIN(Sample)
