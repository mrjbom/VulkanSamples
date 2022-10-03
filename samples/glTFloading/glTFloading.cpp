#include "../../Base/BaseSample.h"

std::string EXAMPLE_NAME_STR = std::string("glTFloading");

class Sample : public BaseSample
{
    vulkanglTF::Model*              model = nullptr;

    struct ShaderData {
        VulkanBuffer vulkanBuffer;
        struct Data {
            glm::mat4 projection = glm::mat4(1.0f);
            glm::mat4 view = glm::mat4(1.0f);
            glm::mat4 model = glm::mat4(1.0f);
        } data;
    };

    // One per frame
    std::vector<ShaderData>         shaderData;

    VkShaderModule                  vertShaderModule = VK_NULL_HANDLE;
    VkShaderModule                  fragShaderModule = VK_NULL_HANDLE;
    VkPipelineLayout                pipelineLayout = VK_NULL_HANDLE;
    VkPipeline                      graphicsPipeline = VK_NULL_HANDLE;

    std::vector<VkDescriptorSet>    descriptorSetsMatrices;
    VkDescriptorSetLayout           descriptorSetLayoutMatrices = VK_NULL_HANDLE;
    VkDescriptorPool                descriptorPool = VK_NULL_HANDLE;
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
        setupDescriptorSetLayout();
        createGraphicsPipeline();
        setupDescriptorPool();
        setupDescriptorSet();
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
        vkDestroyDescriptorSetLayout(base_vulkanDevice->logicalDevice, descriptorSetLayoutMatrices, nullptr);

        // Uniform buffer
        for (uint32_t i = 0; i < BASE_MAX_FRAMES_IN_FLIGHT; ++i) {
            shaderData[i].vulkanBuffer.destroy();
        }

        // Model
        delete model;
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
        //base_camera.type = Camera::CameraType::firstperson;
        base_camera.setPerspective(60.0f, (float)base_windowWidth / (float)base_windowHeight, 0.1f, 100.0f);
        //base_camera.setPosition(glm::vec3(0.0f, 0.0f, -1.0));
        base_camera.setDistance(1.0f);
    }

    void loadAssets()
    {
        model = new vulkanglTF::Model(base_vulkanDevice, base_graphicsQueue, base_commandPoolGraphics, base_vmaAllocator);
        const uint32_t glTFLoadingFlags = vulkanglTF::PreTransformVertices | vulkanglTF::FileLoadingFlags::PreMultiplyVertexColors | vulkanglTF::FileLoadingFlags::FlipZ;
        model->loadFromFile(ASSETS_DATA_PATH + "/models/FlightHelmet/glTF/FlightHelmet.gltf", glTFLoadingFlags, base_graphicsQueue, base_commandPoolGraphics);
        //"/models/BoomBoxWithAxesBlender/BoomBoxWithAxesBlender.gltf" - Tested on left handed coordinate system(with fliping Z load flag)
        //"/models/FlightHelmet/glTF/FlightHelmet.gltf"
    }

    void prepareUniformBuffers()
    {
        shaderData.resize(BASE_MAX_FRAMES_IN_FLIGHT);
        for (uint32_t i = 0; i < BASE_MAX_FRAMES_IN_FLIGHT; ++i) {
            shaderData[i].vulkanBuffer.setDeviceAndAllocator(base_vulkanDevice, base_vmaAllocator);
            shaderData[i].vulkanBuffer.createBuffer(
                sizeof(ShaderData),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
            );
        }
    }

    void setupDescriptorSetLayout()
    {
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
        {
            // Binding 0 : Vertex shader uniform buffer
            vulkanInitializers::descriptorSetLayoutBinding(
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                VK_SHADER_STAGE_VERTEX_BIT,
                0)
        };

        VkDescriptorSetLayoutCreateInfo descriptorLayout =
            vulkanInitializers::descriptorSetLayoutCreateInfo(
                setLayoutBindings.data(),
                setLayoutBindings.size());

        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(base_vulkanDevice->logicalDevice, &descriptorLayout, nullptr, &descriptorSetLayoutMatrices));
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

        std::vector<VkDescriptorSetLayout> descriptorSetLayoutsList = { descriptorSetLayoutMatrices, vulkanglTF::descriptorSetLayoutImage };
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = descriptorSetLayoutsList.size();
        pipelineLayoutInfo.pSetLayouts = descriptorSetLayoutsList.data();

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

    void setupDescriptorPool()
    {
        std::vector<VkDescriptorPoolSize> poolSizes =
        {
            // One descriptor per frame
            vulkanInitializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, BASE_MAX_FRAMES_IN_FLIGHT)
        };

        // One descriptor set per frame
        VkDescriptorPoolCreateInfo descriptorPoolInfo =
            vulkanInitializers::descriptorPoolCreateInfo(
                poolSizes.size(),
                poolSizes.data(),
                BASE_MAX_FRAMES_IN_FLIGHT
            );

        VK_CHECK_RESULT(vkCreateDescriptorPool(base_vulkanDevice->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));
    }

    void setupDescriptorSet()
    {
        // Allocate BASE_MAX_FRAMES_IN_FLIGHT descriptors sets
        // One descriptor set per frame
        std::vector<VkDescriptorSetLayout> setsLayouts(BASE_MAX_FRAMES_IN_FLIGHT, descriptorSetLayoutMatrices);
        VkDescriptorSetAllocateInfo allocInfo =
            vulkanInitializers::descriptorSetAllocateInfo(
                descriptorPool,
                setsLayouts.data(),
                BASE_MAX_FRAMES_IN_FLIGHT);

        descriptorSetsMatrices.resize(BASE_MAX_FRAMES_IN_FLIGHT);
        VK_CHECK_RESULT(vkAllocateDescriptorSets(base_vulkanDevice->logicalDevice, &allocInfo, descriptorSetsMatrices.data()));

        for (uint32_t i = 0; i < BASE_MAX_FRAMES_IN_FLIGHT; ++i) {
            VkWriteDescriptorSet writeDescriptorSet =
            {
                // Binding 0 : Vertex shader uniform buffer
                vulkanInitializers::writeDescriptorSet(
                    descriptorSetsMatrices[i],
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    0,
                    &shaderData[i].vulkanBuffer.descriptor,
                    1)
            };

            vkUpdateDescriptorSets(base_vulkanDevice->logicalDevice, 1, &writeDescriptorSet, 0, nullptr);
        }
    }

    void updateUniformBuffers(uint32_t currentFrame)
    {
        base_camera.updateAspectRatio((float)base_windowWidth / (float)base_windowHeight);
        base_camera.update((float)base_frameTime / 1000.0f);

        glm::mat4 model_transform = glm::mat4(1.0f);
        shaderData[currentFrame].data.projection = base_camera.matrices.perspective;
        shaderData[currentFrame].data.view = base_camera.matrices.view;
        shaderData[currentFrame].data.model = model_transform;

        void* pMappedBuffer = nullptr;
        shaderData[currentFrame].vulkanBuffer.map(&pMappedBuffer);
        memcpy(pMappedBuffer, &shaderData[currentFrame].data, sizeof(ShaderData::Data));
        shaderData[currentFrame].vulkanBuffer.unmap();
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
        viewport.y = static_cast<float>(base_vulkanSwapChain->surfaceExtent.height);;
        viewport.width = static_cast<float>(base_vulkanSwapChain->surfaceExtent.width);
        viewport.height = -static_cast<float>(base_vulkanSwapChain->surfaceExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = base_vulkanSwapChain->surfaceExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        // Bind scene matrices descriptor to set 0
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSetsMatrices[base_currentFrameIndex], 0, nullptr);
        model->bindBuffers(commandBuffer);
        model->draw(commandBuffer, vulkanglTF::RenderFlags::BindImages, pipelineLayout, 1);

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
