#include "../../Base/BaseSample.h"

std::string EXAMPLE_NAME_STR = std::string("TextureMapping");

class Sample : public BaseSample
{
    // Input vertexes
    struct Vertex
    {
        glm::vec3 position;
        glm::vec2 textureCoord;
    };
    VulkanBuffer                    vertexBuffer;

    VulkanBuffer                    indexBuffer;

    VulkanTexture2D                 vulkanTexture{};

    VkDescriptorPool                descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet                 descriptorSet;
    VkDescriptorSetLayout           descriptorSetLayout;

    // Matrixes
    struct PushConstantData
    {
        alignas(16) glm::mat4 MVPmatrix;
        alignas(16) float lodBias = 0;
    };
    PushConstantData pushConstantData{};

    // TODO
    // Study this in more detail, since in OpenGL the coordinates start from a different angle
    // Are they exactly different in Vulkan?
    // UV Texture Coords
    /*
    *  (0;0)            1
    *    *------------>
    *    |              U-coord
    *    |
    *    |
    *    |
    *    |
    *  1 V V-coord
    */
    
    std::vector<Vertex> vertexes = {
        { glm::vec3(-0.5, 0.5, 0.0), glm::vec2(0.0, 0.0) },
        { glm::vec3(0.5, 0.5, 0.0), glm::vec2(1.0, 0.0) },
        { glm::vec3(0.5, -0.5, 0.0), glm::vec2(1.0, 1.0) },
        { glm::vec3(-0.5, -0.5, 0.0), glm::vec2(0.0, 1.0) }
    };

    std::vector<uint16_t> indexes = {
        0, 1, 2, 2, 3, 0
    };

    VkShaderModule      vertShaderModule = VK_NULL_HANDLE;
    VkShaderModule      fragShaderModule = VK_NULL_HANDLE;
    VkPipelineLayout    pipelineLayout = VK_NULL_HANDLE;
    VkPipeline          graphicsPipeline = VK_NULL_HANDLE;

public:
    Sample()
    {
        // Setting sample requirements
        base_title = "Texture Mapping";
    }

    void prepare()
    {
        BaseSample::prepare();
        initCamera();
        loadTexture();
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

        // Descriptors data
        vkDestroyDescriptorPool(base_vulkanDevice->logicalDevice, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(base_vulkanDevice->logicalDevice, descriptorSetLayout, nullptr);

        // Buffers
        indexBuffer.destroy();
        vertexBuffer.destroy();

        // Texture
        vulkanTexture.destroy();
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
        base_camera.setPerspective(45.0f, (float)base_windowWidth / (float)base_windowHeight, 0.1f, 100.0f);
        base_camera.setPosition(glm::vec3(0.0f, 0.0f, -2.0f));
    }

    void loadTexture()
    {
        // We use the Khronos texture format (https://www.khronos.org/opengles/sdk/tools/KTX/file_format_spec/)
        std::string filePath = ASSETS_DATA_PATH + "/textures/mipmapsTest.ktx";

        /*
        vulkanTexture.setDeviceAndAllocator(base_vulkanDevice, base_vmaAllocator);
        vulkanTexture.createTextureFromKTX(
            base_graphicsQueue,
            base_commandPoolGraphics,
            filePath
        );
        */

        ///*
        // We create the image in the local memory of the device(without the possibility of mapping to the host memory)
        // and use an staging buffer to copy the texture data to image memory

        // Texture data contains 4 channels (RGBA) with unnormalized 8-bit values, this is the most commonly supported format
        VkFormat textureFormat = VK_FORMAT_R8G8B8A8_UNORM;
        
        ktxResult result;
        ktxTexture* ktxTexture;

        if (!vulkanTools::fileExists(filePath)) {
            throw MakeErrorInfo("File " + filePath + " not exist! Check assets files!");
        }

        result = ktxTexture_CreateFromNamedFile(filePath.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
        if (result != KTX_SUCCESS) {
            throw MakeErrorInfo("ktx: failed to load texture!");
        }

        vulkanTexture.vulkanDevice = base_vulkanDevice;
        vulkanTexture.vmaAllocator = base_vmaAllocator;

        // Get texture properties 
        vulkanTexture.width = ktxTexture->baseWidth;
        vulkanTexture.height = ktxTexture->baseHeight;
        vulkanTexture.mipLevels = ktxTexture->numLevels;
        ktx_uint8_t* ktxTextureData = ktxTexture_GetData(ktxTexture);
        ktx_size_t ktxTextureSize = ktxTexture_GetDataSize(ktxTexture);

        // Create staging buffer
        VulkanBuffer stagingBuffer(base_vulkanDevice, base_vmaAllocator);
        stagingBuffer.createBuffer(
            ktxTextureSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            ktxTextureData,
            ktxTextureSize
        );

        // Creating image
        VkImageCreateInfo imageCreateInfo{};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = textureFormat;
        imageCreateInfo.extent = { vulkanTexture.width, vulkanTexture.height, 1 };
        imageCreateInfo.mipLevels = vulkanTexture.mipLevels;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        
        VmaAllocationCreateInfo imageAllocationCreateInfo{};
        imageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        imageAllocationCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        if (vmaCreateImage(base_vmaAllocator, &imageCreateInfo, &imageAllocationCreateInfo, &vulkanTexture.image, &vulkanTexture.vmaImageAllocation, &vulkanTexture.vmaImageAllocationInfo) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to create image!");
        }

        // The graphical queue implicitly supports transfer operations
        VkCommandBuffer commandBuffer = base_vulkanDevice->beginSingleTimeCommands(base_commandPoolGraphics);
        
        // The sub resource range describes the regions of the image that will be transitioned using the memory barriers below
        VkImageSubresourceRange subresourceRange = {};
        // Image only contains color data
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        // Start at first mip level
        subresourceRange.baseMipLevel = 0;
        // We will transition on all mip levels
        subresourceRange.levelCount = vulkanTexture.mipLevels;
        // The 2D texture only has one layer
        subresourceRange.baseArrayLayer = 0;
        subresourceRange.layerCount = 1;

        // Transition the texture image layout to transfer target, so we can safely copy our buffer data to it
        vulkanTools::insertImageMemoryBarrier(
            commandBuffer,
            vulkanTexture.image,
            0, // srcAccessMask - We do not perform any operations before memory barrier
            VK_ACCESS_TRANSFER_WRITE_BIT, // dstAccessMask - We write after memory barrier
            VK_IMAGE_LAYOUT_UNDEFINED, // oldImageLayout
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // newImageLayout
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // srcStageMask - We don't wait anything before the barrier
            VK_PIPELINE_STAGE_TRANSFER_BIT, // dstStageMask - Stages in which we make transfer operations should wait a barrier
            subresourceRange);

        // Copy buffer to image
        
        // Setup buffer copy regions for each mip level
        std::vector<VkBufferImageCopy> bufferCopyRegions;
        for (uint32_t i = 0; i < vulkanTexture.mipLevels; i++) {
            ktx_size_t offset;
            KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, i, 0, 0, &offset);
            assert(ret == KTX_SUCCESS);
            // Setup a buffer image copy structure for the current mip level
            VkBufferImageCopy bufferCopyRegion = {};
            bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel = i;
            bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
            bufferCopyRegion.imageSubresource.layerCount = 1;
            bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth >> i;
            bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight >> i;
            bufferCopyRegion.imageExtent.depth = 1;
            bufferCopyRegion.bufferOffset = offset;
            bufferCopyRegions.push_back(bufferCopyRegion);
        }

        // Copy mip levels from staging buffer
        vkCmdCopyBufferToImage(
            commandBuffer,
            stagingBuffer.buffer,
            vulkanTexture.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            bufferCopyRegions.size(),
            bufferCopyRegions.data());

        // Change image layout to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL after transfer
        vulkanTools::insertImageMemoryBarrier(
            commandBuffer,
            vulkanTexture.image,
            VK_ACCESS_TRANSFER_WRITE_BIT, // srcAccessMask - We write the data to the image
            VK_ACCESS_SHADER_READ_BIT, // dstAccessMask - The shader reads data from the image
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // oldImageLayout
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, // newImageLayout
            VK_PIPELINE_STAGE_TRANSFER_BIT, // srcStageMask - We have to wait for the transfer operation that loads the image
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // dstStageMask - The shader should read the data only after it is loaded to image
            subresourceRange);

        // Store current layout for later reuse
        vulkanTexture.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        base_vulkanDevice->endSingleTimeCommands(commandBuffer, base_graphicsQueue, base_commandPoolGraphics);

        // Destroy staging buffer
        stagingBuffer.destroy();
        ktxTexture_Destroy(ktxTexture);

        // Create image view
        VkImageViewCreateInfo imageViewCreateInfo{};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.image = vulkanTexture.image;
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = textureFormat;
        imageViewCreateInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = vulkanTexture.mipLevels;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(base_vulkanDevice->logicalDevice, &imageViewCreateInfo, nullptr, &vulkanTexture.imageView) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to create image view!");
        }

        // Create sampler
        // The shader accesses the texture using the sampler
        VkSamplerCreateInfo samplerCreateInfo{};
        samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        if (base_vulkanDevice->enabledFeatures.samplerAnisotropy == VK_TRUE) {
            samplerCreateInfo.anisotropyEnable = VK_TRUE;
            samplerCreateInfo.maxAnisotropy = base_vulkanDevice->properties.limits.maxSamplerAnisotropy;
        }
        else {
            samplerCreateInfo.anisotropyEnable = VK_FALSE;
            samplerCreateInfo.maxAnisotropy = 1.0f;
        }
        samplerCreateInfo.compareEnable = VK_FALSE;
        samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
        samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCreateInfo.mipLodBias = 0.0f;
        samplerCreateInfo.minLod = 0.0f;
        samplerCreateInfo.maxLod = (float)vulkanTexture.mipLevels;
        samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

        if (vkCreateSampler(base_vulkanDevice->logicalDevice, &samplerCreateInfo, nullptr, &vulkanTexture.sampler) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to create sampler!");
        }
        //*/
    }

    void createBuffers()
    {
        // Vertex buffer
        vertexBuffer.setDeviceAndAllocator(base_vulkanDevice, base_vmaAllocator);
        vertexBuffer.createBuffer(
            sizeof(vertexes[0]) * vertexes.size(),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            vertexes.data(),
            sizeof(vertexes[0]) * vertexes.size()
        );

        // Indixes buffer
        indexBuffer.setDeviceAndAllocator(base_vulkanDevice, base_vmaAllocator);
        indexBuffer.createBuffer(
            sizeof(indexes[0]) * indexes.size(),
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            indexes.data(),
            sizeof(indexes[0]) * indexes.size()
        );
    }

    void createDescriptorSetLayouts()
    {
        // Binding for descriptor set
        VkDescriptorSetLayoutBinding layoutBinding{};
        layoutBinding.binding = 0;
        layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        layoutBinding.descriptorCount = 1;
        layoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
        descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCreateInfo.bindingCount = 1;
        descriptorSetLayoutCreateInfo.pBindings = &layoutBinding;

        if (vkCreateDescriptorSetLayout(base_vulkanDevice->logicalDevice, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to create descriptor set!");
        }
    }

    void createDescriptorSets()
    {
        VkDescriptorPoolSize descriptorPoolSize{};
        descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorPoolSize.descriptorCount = 1;

        VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
        descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolCreateInfo.poolSizeCount = 1;
        descriptorPoolCreateInfo.pPoolSizes = &descriptorPoolSize;
        descriptorPoolCreateInfo.maxSets = 1;

        if (vkCreateDescriptorPool(base_vulkanDevice->logicalDevice, &descriptorPoolCreateInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to create descriptor pool!");
        }

        // Allocate descriptor sets(one descriptor set per frame)
        VkDescriptorSetAllocateInfo setsAllocInfo{};
        setsAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        setsAllocInfo.descriptorPool = descriptorPool;
        setsAllocInfo.descriptorSetCount = 1;
        setsAllocInfo.pSetLayouts = &descriptorSetLayout;

        if (vkAllocateDescriptorSets(base_vulkanDevice->logicalDevice, &setsAllocInfo, &descriptorSet) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to allocate descriptor sets!");
        }

        // Write descriptor
        vulkanTexture.updateDescriptor();

        VkWriteDescriptorSet writeDescriptorSet{};
        writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSet.dstSet = descriptorSet;
        writeDescriptorSet.dstBinding = 0;
        writeDescriptorSet.dstArrayElement = 0;
        writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeDescriptorSet.descriptorCount = 1;
        writeDescriptorSet.pImageInfo = &vulkanTexture.descriptor;

        vkUpdateDescriptorSets(base_vulkanDevice->logicalDevice, 1, &writeDescriptorSet, 0, nullptr);
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
        attributeDescriptions.back().format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions.back().offset = offsetof(Vertex, textureCoord);

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
        vertexMatrixPushConstantRange.size = sizeof(PushConstantData);

        std::vector<VkPushConstantRange> pushConstantsRanges{ vertexMatrixPushConstantRange };

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
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

    void updatePushConstants()
    {
        base_camera.updateAspectRatio((float)base_windowWidth / (float)base_windowHeight);
        base_camera.update((float)base_frameTime / 1000.0f);
        pushConstantData.MVPmatrix = base_camera.matrices.perspective * base_camera.matrices.view;
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

        std::vector<VkDeviceSize> offsets(vertexes.size(), 0);
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer.buffer, offsets.data());
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantData), &pushConstantData);
        
        vkCmdDrawIndexed(commandBuffer, 6, 1, 0, 0, 0);

        vkCmdEndRenderPass(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to record command buffer!");
        }
    }

    void drawUI()
    {
        UIOverlay::windowBegin(base_title.c_str(), nullptr, { 0, 0 }, { 300, 100 });
        UIOverlay::printFPS((float)base_frameTime, 500);
        ImGui::SliderFloat("LOD bias", &pushConstantData.lodBias, 0.0f, (float)vulkanTexture.mipLevels);
        UIOverlay::windowEnd();
    }
};

EXAMPLE_MAIN(Sample)
