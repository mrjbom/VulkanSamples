/*
 * Copyright (C) 2020 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE

#include "VulkanglTFModel.h"

uint32_t vulkanglTF::descriptorBindingFlags = vulkanglTF::DescriptorBindingFlags::ImageBaseColor;
VkDescriptorSetLayout vulkanglTF::descriptorSetLayoutImage = VK_NULL_HANDLE;

vulkanglTF::Model::Model(VulkanDevice* vulkanDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, VmaAllocator vmaAllocator)
{
    this->vulkanDevice = vulkanDevice;
    this->transferQueue = transferQueue;
    this->transferCommandPool = transferCommandPool;
    this->vmaAllocator = vmaAllocator;
}

vulkanglTF::Model::~Model()
{
    if (descriptorSetLayoutImage) {
        vkDestroyDescriptorSetLayout(vulkanDevice->logicalDevice, descriptorSetLayoutImage, nullptr);
    }

    if (descriptorPool) {
        vkDestroyDescriptorPool(vulkanDevice->logicalDevice, descriptorPool, nullptr);
    }

    for (auto texture : textures) {
        texture.destroy();
    }

    emptyTexture.destroy();

    for (auto node : nodes) {
        delete node;
    }

    if (vertexBuffer.vulkanBuffer) {
        vertexBuffer.vulkanBuffer->destroy();
        delete vertexBuffer.vulkanBuffer;
    }

    if (indexBuffer.vulkanBuffer) {
        indexBuffer.vulkanBuffer->destroy();
        delete indexBuffer.vulkanBuffer;
    }
}

void vulkanglTF::Model::loadImages(tinygltf::Model& gltfModel, VulkanDevice* device, VkQueue transferQueue)
{
    for (tinygltf::Image& image : gltfModel.images) {
        Texture texture;
        texture.fromglTfImage(image, path, device, transferQueue, transferCommandPool, this->vmaAllocator);
        textures.push_back(texture);
    }
    // Create an empty texture to be used for empty material images
    createEmptyTexture(transferQueue);
}

vulkanglTF::Mesh::Mesh(VulkanDevice* vulkanDevice, VmaAllocator vmaAllocator, glm::mat4 matrix) {
    this->vulkanDevice = vulkanDevice;
    this->vmaAllocator = vmaAllocator;
    this->uniformBlock.matrix = matrix;

    this->uniformBuffer.vulkanBuffer = new VulkanBuffer(vulkanDevice, vmaAllocator);
    this->uniformBuffer.vulkanBuffer->createBuffer(
        sizeof(uniformBlock),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
    );
};

vulkanglTF::Mesh::~Mesh() {
    for (auto& primitive : primitives) {
        delete primitive;
    }
    vmaDestroyBuffer(vmaAllocator, uniformBuffer.vulkanBuffer->buffer, uniformBuffer.vulkanBuffer->vmaAllocation);
    delete uniformBuffer.vulkanBuffer;
}

void vulkanglTF::Material::createDescriptorSet(VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout, uint32_t descriptorBindingFlags)
{
    VkDescriptorSetAllocateInfo descriptorSetAllocInfo{};
    descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocInfo.descriptorPool = descriptorPool;
    descriptorSetAllocInfo.pSetLayouts = &descriptorSetLayout;
    descriptorSetAllocInfo.descriptorSetCount = 1;
    VK_CHECK_RESULT(vkAllocateDescriptorSets(vulkanDevice->logicalDevice, &descriptorSetAllocInfo, &descriptorSet));
    std::vector<VkDescriptorImageInfo> imageDescriptors{};
    std::vector<VkWriteDescriptorSet> writeDescriptorSets{};
    if (descriptorBindingFlags & DescriptorBindingFlags::ImageBaseColor) {
        imageDescriptors.push_back(baseColorTexture->descriptor);
        VkWriteDescriptorSet writeDescriptorSet{};
        writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeDescriptorSet.descriptorCount = 1;
        writeDescriptorSet.dstSet = descriptorSet;
        writeDescriptorSet.dstBinding = static_cast<uint32_t>(writeDescriptorSets.size());
        writeDescriptorSet.pImageInfo = &baseColorTexture->descriptor;
        writeDescriptorSets.push_back(writeDescriptorSet);
    }
    if (normalTexture && descriptorBindingFlags & DescriptorBindingFlags::ImageNormalMap) {
        imageDescriptors.push_back(normalTexture->descriptor);
        VkWriteDescriptorSet writeDescriptorSet{};
        writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeDescriptorSet.descriptorCount = 1;
        writeDescriptorSet.dstSet = descriptorSet;
        writeDescriptorSet.dstBinding = static_cast<uint32_t>(writeDescriptorSets.size());
        writeDescriptorSet.pImageInfo = &normalTexture->descriptor;
        writeDescriptorSets.push_back(writeDescriptorSet);
    }
    vkUpdateDescriptorSets(vulkanDevice->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
}

void vulkanglTF::Texture::updateDescriptor()
{
    descriptor.sampler = sampler;
    descriptor.imageView = imageView;
    descriptor.imageLayout = imageLayout;
}

void vulkanglTF::Texture::destroy()
{
    if (vulkanDevice)
    {
        vkDestroySampler(vulkanDevice->logicalDevice, sampler, nullptr);
        vkDestroyImageView(vulkanDevice->logicalDevice, imageView, nullptr);
        vmaDestroyImage(vmaAllocator, image, vmaAllocation);
    }
}

void vulkanglTF::Texture::fromglTfImage(tinygltf::Image& gltfimage, std::string path, VulkanDevice* vulkanDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, VmaAllocator vmaAllocator)
{
    this->vulkanDevice = vulkanDevice;
    this->vmaAllocator = vmaAllocator;
    this->width = gltfimage.width;
    this->height = gltfimage.height;
    this->layerCount = 1;
    this->mipLevels = 1;

    unsigned char* buffer = nullptr;
    size_t bufferSize = 0;
    bool deleteBuffer = false;
    if (gltfimage.component == 3) {
        // Most devices don't support RGB only on Vulkan so convert if necessary
        // TODO: Check actual format support and transform only if required
        bufferSize = gltfimage.width * gltfimage.height * 4;
        buffer = new unsigned char[bufferSize];
        unsigned char* rgba = buffer;
        unsigned char* rgb = &gltfimage.image[0];
        for (int i = 0; i < gltfimage.width * gltfimage.height; ++i) {
            for (int32_t j = 0; j < 3; ++j) {
                rgba[j] = rgb[j];
            }
            rgba += 4;
            rgb += 3;
        }
        deleteBuffer = true;
    }
    else {
        buffer = &gltfimage.image[0];
        bufferSize = gltfimage.image.size();
    }

    uint32_t imageSize = width * height * 4;
    unsigned char* imageData = buffer;

    // Create staging buffer
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation stagingBufferAllocation = 0;
    VmaAllocationInfo stagingBufferAllocationInfo{};

    VkBufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = imageSize;
    // This buffer is used as a transfer source for the buffer copy
    bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo bufferAllocationCreateInfo{};
    bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    bufferAllocationCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    bufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT; // We must use memcpy (not random access!)

    if (vmaCreateBuffer(vmaAllocator, &bufferCreateInfo, &bufferAllocationCreateInfo, &stagingBuffer, &stagingBufferAllocation, &stagingBufferAllocationInfo) != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to create buffer!");
    }

    // Map buffer memory and copy image data
    void* mappedBufferData = nullptr;
    if (vmaMapMemory(vmaAllocator, stagingBufferAllocation, &mappedBufferData)) {
        throw MakeErrorInfo("Failed to map buffer!");
    }
    memcpy(mappedBufferData, imageData, (size_t)bufferCreateInfo.size);
    vmaUnmapMemory(vmaAllocator, stagingBufferAllocation);

    // Create image
    VkImageCreateInfo imageCreateInfo{};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = this->textureFormat;
    imageCreateInfo.extent = { this->width, this->height, 1 };
    imageCreateInfo.mipLevels = this->mipLevels;
    imageCreateInfo.arrayLayers = this->layerCount;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo imageAllocationCreateInfo{};
    bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    bufferAllocationCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    if (vmaCreateImage(vmaAllocator, &imageCreateInfo, &imageAllocationCreateInfo, &this->image, &this->vmaAllocation, &this->vmaAllocationInfo) != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to create image!");
    }

    // Copy image data from buffer to image
    VkCommandBuffer commandBuffer = vulkanDevice->beginSingleTimeCommands(transferCommandPool);

    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = this->mipLevels;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;

    // Transition the texture image layout to transfer target, so we can safely copy our buffer data to it
    vulkanTools::insertImageMemoryBarrier(
        commandBuffer,
        this->image,
        0, // srcAccessMask - We do not perform any operations before memory barrier
        VK_ACCESS_TRANSFER_WRITE_BIT, // dstAccessMask - We write after memory barrier
        VK_IMAGE_LAYOUT_UNDEFINED, // oldImageLayout
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // newImageLayout
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // srcStageMask - We don't wait anything before the barrier
        VK_PIPELINE_STAGE_TRANSFER_BIT, // dstStageMask - Stages in which we make transfer operations should wait a barrier
        subresourceRange
    );

    // Copy buffer to image
    // Setup buffer copy regions without mip levels and layers
    VkBufferImageCopy bufferCopyRegion{};
    bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferCopyRegion.imageSubresource.mipLevel = 0;
    bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
    bufferCopyRegion.imageSubresource.layerCount = 1;
    bufferCopyRegion.imageExtent.width = this->width;
    bufferCopyRegion.imageExtent.height = this->height;
    bufferCopyRegion.imageExtent.depth = 1;
    bufferCopyRegion.bufferOffset = 0;

    // Copy image from staging buffer
    vkCmdCopyBufferToImage(
        commandBuffer,
        stagingBuffer,
        this->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &bufferCopyRegion
    );

    // Change image layout to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL after transfer
    vulkanTools::insertImageMemoryBarrier(
        commandBuffer,
        this->image,
        VK_ACCESS_TRANSFER_WRITE_BIT, // srcAccessMask - We write the data to the image
        VK_ACCESS_SHADER_READ_BIT, // dstAccessMask - The shader reads data from the image
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // oldImageLayout
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, // newImageLayout
        VK_PIPELINE_STAGE_TRANSFER_BIT, // srcStageMask - We have to wait for the transfer operation that loads the image
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // dstStageMask - The shader should read the data only after it is loaded to image
        subresourceRange
    );

    // Store current layout for later reuse
    this->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    vulkanDevice->endSingleTimeCommands(commandBuffer, transferQueue, transferCommandPool);
    // Destroy staging buffer
    vmaDestroyBuffer(vmaAllocator, stagingBuffer, stagingBufferAllocation);

    // Create image view
    VkImageViewCreateInfo imageViewCreateInfo{};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.image = this->image;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = this->textureFormat;
    imageViewCreateInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = this->mipLevels;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(vulkanDevice->logicalDevice, &imageViewCreateInfo, nullptr, &this->imageView) != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to create image view!");
    }

    // Create sampler
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = 0.0f;
    samplerCreateInfo.anisotropyEnable = VK_FALSE;
    samplerCreateInfo.maxAnisotropy = 1.0f;

    if (vkCreateSampler(vulkanDevice->logicalDevice, &samplerCreateInfo, nullptr, &this->sampler) != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to create sampler!");
    }

    descriptor.sampler = this->sampler;
    descriptor.imageView = this->imageView;
    descriptor.imageLayout = this->imageLayout;
}

glm::mat4 vulkanglTF::Node::localMatrix()
{
    return glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation) * glm::scale(glm::mat4(1.0f), scale) * matrix;
}

glm::mat4 vulkanglTF::Node::getMatrix()
{
    glm::mat4 m = localMatrix();
    vulkanglTF::Node* p = parent;
    while (p) {
        m = p->localMatrix() * m;
        p = p->parent;
    }
    return m;
}

void vulkanglTF::Node::update()
{
    if (mesh) {
        glm::mat4 m = getMatrix();
        void* mappedBuffer;
        mesh->uniformBuffer.vulkanBuffer->map(&mappedBuffer);
        memcpy(mappedBuffer, &m, sizeof(glm::mat4));
        mesh->uniformBuffer.vulkanBuffer->unmap();
    }

    for (auto& child : children) {
        child->update();
    }
}

vulkanglTF::Node::~Node()
{
    if (mesh) {
        delete mesh;
    }
    for (auto& child : children) {
        delete child;
    }
}

vulkanglTF::Texture* vulkanglTF::Model::getTexture(uint32_t index)
{
    if (index >= textures.size()) {
        return nullptr;
    }
    return &textures[index];
}

void vulkanglTF::Model::createEmptyTexture(VkQueue transferQueue)
{
    emptyTexture.vmaAllocator = vmaAllocator;
    emptyTexture.vulkanDevice = vulkanDevice;
    emptyTexture.width = 1;
    emptyTexture.height = 1;
    emptyTexture.layerCount = 1;
    emptyTexture.mipLevels = 1;

    uint32_t imageSize = emptyTexture.width * emptyTexture.height * 4;
    unsigned char* imageData = new unsigned char[imageSize];
    memset(imageData, 0, imageSize);

    // Create staging buffer
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation stagingBufferAllocation = 0;
    VmaAllocationInfo stagingBufferAllocationInfo{};

    VkBufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = imageSize;
    // This buffer is used as a transfer source for the buffer copy
    bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo bufferAllocationCreateInfo{};
    bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    bufferAllocationCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    bufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT; // We must use memcpy (not random access!)

    if (vmaCreateBuffer(vmaAllocator, &bufferCreateInfo, &bufferAllocationCreateInfo, &stagingBuffer, &stagingBufferAllocation, &stagingBufferAllocationInfo) != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to create buffer!");
    }

    // Map buffer memory and copy image data
    void* mappedBufferData = nullptr;
    if (vmaMapMemory(vmaAllocator, stagingBufferAllocation, &mappedBufferData)) {
        throw MakeErrorInfo("Failed to map buffer!");
    }
    memcpy(mappedBufferData, imageData, (size_t)bufferCreateInfo.size);
    vmaUnmapMemory(vmaAllocator, stagingBufferAllocation);
    delete[] imageData;

    // Create image
    VkImageCreateInfo imageCreateInfo{};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = emptyTexture.textureFormat;
    imageCreateInfo.extent = { emptyTexture.width, emptyTexture.height, 1 };
    imageCreateInfo.mipLevels = emptyTexture.mipLevels;
    imageCreateInfo.arrayLayers = emptyTexture.layerCount;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo imageAllocationCreateInfo{};
    bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    bufferAllocationCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    if (vmaCreateImage(vmaAllocator, &imageCreateInfo, &imageAllocationCreateInfo, &emptyTexture.image, &emptyTexture.vmaAllocation, &emptyTexture.vmaAllocationInfo) != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to create image!");
    }

    // Copy image data from buffer to image
    VkCommandBuffer commandBuffer = vulkanDevice->beginSingleTimeCommands(transferCommandPool);

    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = emptyTexture.mipLevels;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;

    // Transition the texture image layout to transfer target, so we can safely copy our buffer data to it
    vulkanTools::insertImageMemoryBarrier(
        commandBuffer,
        emptyTexture.image,
        0, // srcAccessMask - We do not perform any operations before memory barrier
        VK_ACCESS_TRANSFER_WRITE_BIT, // dstAccessMask - We write after memory barrier
        VK_IMAGE_LAYOUT_UNDEFINED, // oldImageLayout
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // newImageLayout
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // srcStageMask - We don't wait anything before the barrier
        VK_PIPELINE_STAGE_TRANSFER_BIT, // dstStageMask - Stages in which we make transfer operations should wait a barrier
        subresourceRange
    );

    // Copy buffer to image
    // Setup buffer copy regions without mip levels and layers
    VkBufferImageCopy bufferCopyRegion{};
    bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferCopyRegion.imageSubresource.mipLevel = 0;
    bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
    bufferCopyRegion.imageSubresource.layerCount = 1;
    bufferCopyRegion.imageExtent.width = emptyTexture.width;
    bufferCopyRegion.imageExtent.height = emptyTexture.height;
    bufferCopyRegion.imageExtent.depth = 1;
    bufferCopyRegion.bufferOffset = 0;

    // Copy image from staging buffer
    vkCmdCopyBufferToImage(
        commandBuffer,
        stagingBuffer,
        emptyTexture.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &bufferCopyRegion
    );

    // Change image layout to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL after transfer
    vulkanTools::insertImageMemoryBarrier(
        commandBuffer,
        emptyTexture.image,
        VK_ACCESS_TRANSFER_WRITE_BIT, // srcAccessMask - We write the data to the image
        VK_ACCESS_SHADER_READ_BIT, // dstAccessMask - The shader reads data from the image
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // oldImageLayout
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, // newImageLayout
        VK_PIPELINE_STAGE_TRANSFER_BIT, // srcStageMask - We have to wait for the transfer operation that loads the image
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // dstStageMask - The shader should read the data only after it is loaded to image
        subresourceRange
    );

    // Store current layout for later reuse
    emptyTexture.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    vulkanDevice->endSingleTimeCommands(commandBuffer, transferQueue, transferCommandPool);
    // Destroy staging buffer
    vmaDestroyBuffer(vmaAllocator, stagingBuffer, stagingBufferAllocation);

    // Create image view
    VkImageViewCreateInfo imageViewCreateInfo{};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.image = emptyTexture.image;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = emptyTexture.textureFormat;
    imageViewCreateInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = emptyTexture.mipLevels;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(vulkanDevice->logicalDevice, &imageViewCreateInfo, nullptr, &emptyTexture.imageView) != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to create image view!");
    }

    // Create sampler
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = 0.0f;
    samplerCreateInfo.anisotropyEnable = VK_FALSE;
    samplerCreateInfo.maxAnisotropy = 1.0f;

    if (vkCreateSampler(vulkanDevice->logicalDevice, &samplerCreateInfo, nullptr, &emptyTexture.sampler) != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to create sampler!");
    }
}

void vulkanglTF::Model::loadMaterials(tinygltf::Model& gltfModel)
{
    for (tinygltf::Material& mat : gltfModel.materials) {
        Material material(vulkanDevice);
        if (mat.values.find("baseColorTexture") != mat.values.end()) {
            material.baseColorTexture = getTexture(gltfModel.textures[mat.values["baseColorTexture"].TextureIndex()].source);
        }
        // Metallic roughness workflow
        if (mat.values.find("metallicRoughnessTexture") != mat.values.end()) {
            material.metallicRoughnessTexture = getTexture(gltfModel.textures[mat.values["metallicRoughnessTexture"].TextureIndex()].source);
        }
        if (mat.values.find("roughnessFactor") != mat.values.end()) {
            material.roughnessFactor = static_cast<float>(mat.values["roughnessFactor"].Factor());
        }
        if (mat.values.find("metallicFactor") != mat.values.end()) {
            material.metallicFactor = static_cast<float>(mat.values["metallicFactor"].Factor());
        }
        if (mat.values.find("baseColorFactor") != mat.values.end()) {
            material.baseColorFactor = glm::make_vec4(mat.values["baseColorFactor"].ColorFactor().data());
        }
        if (mat.additionalValues.find("normalTexture") != mat.additionalValues.end()) {
            material.normalTexture = getTexture(gltfModel.textures[mat.additionalValues["normalTexture"].TextureIndex()].source);
        }
        else {
            material.normalTexture = &emptyTexture;
        }
        if (mat.additionalValues.find("emissiveTexture") != mat.additionalValues.end()) {
            material.emissiveTexture = getTexture(gltfModel.textures[mat.additionalValues["emissiveTexture"].TextureIndex()].source);
        }
        if (mat.additionalValues.find("occlusionTexture") != mat.additionalValues.end()) {
            material.occlusionTexture = getTexture(gltfModel.textures[mat.additionalValues["occlusionTexture"].TextureIndex()].source);
        }
        if (mat.additionalValues.find("alphaMode") != mat.additionalValues.end()) {
            tinygltf::Parameter param = mat.additionalValues["alphaMode"];
            if (param.string_value == "BLEND") {
                material.alphaMode = Material::ALPHAMODE_BLEND;
            }
            if (param.string_value == "MASK") {
                material.alphaMode = Material::ALPHAMODE_MASK;
            }
        }
        if (mat.additionalValues.find("alphaCutoff") != mat.additionalValues.end()) {
            material.alphaCutoff = static_cast<float>(mat.additionalValues["alphaCutoff"].Factor());
        }

        materials.push_back(material);
    }
    // Push a default material at the end of the list for meshes with no material assigned
    materials.push_back(Material(vulkanDevice));
}

void vulkanglTF::Model::loadNode(Node* parent, const tinygltf::Node& node, uint32_t nodeIndex, const tinygltf::Model& model, std::vector<uint32_t>& indexBuffer, std::vector<Vertex>& vertexBuffer, float globalScale)
{
    Node* newNode = new Node{};
    newNode->parent = parent;
    newNode->name = node.name;
    newNode->matrix = glm::mat4(1.0f);

    // Generate local node matrix
    glm::vec3 translation = glm::vec3(0.0f);
    if (node.translation.size() == 3) {
        translation = glm::make_vec3(node.translation.data());
        newNode->translation = translation;
    }
    glm::mat4 rotation = glm::mat4(1.0f);
    if (node.rotation.size() == 4) {
        glm::quat q = glm::make_quat(node.rotation.data());
        newNode->rotation = glm::mat4(q);
    }
    glm::vec3 scale = glm::vec3(1.0f);
    if (node.scale.size() == 3) {
        scale = glm::make_vec3(node.scale.data());
        newNode->scale = scale;
    }
    if (node.matrix.size() == 16) {
        newNode->matrix = glm::make_mat4x4(node.matrix.data());
    };

    // Node with children
    if (node.children.size() > 0) {
        for (size_t i = 0; i < node.children.size(); i++) {
            loadNode(newNode, model.nodes[node.children[i]], node.children[i], model, indexBuffer, vertexBuffer, globalScale);
        }
    }

    // Node contains mesh data
    if (node.mesh > -1) {
        const tinygltf::Mesh mesh = model.meshes[node.mesh];
        Mesh* newMesh = new Mesh(vulkanDevice, vmaAllocator, newNode->matrix);
        newMesh->name = mesh.name;
        for (size_t j = 0; j < mesh.primitives.size(); j++) {
            const tinygltf::Primitive& primitive = mesh.primitives[j];
            if (primitive.indices < 0) {
                continue;
            }
            uint32_t indexStart = static_cast<uint32_t>(indexBuffer.size());
            uint32_t vertexStart = static_cast<uint32_t>(vertexBuffer.size());
            uint32_t indexCount = 0;
            uint32_t vertexCount = 0;
            glm::vec3 posMin{};
            glm::vec3 posMax{};
            bool hasSkin = false;
            // Vertices
            {
                const float* bufferPos = nullptr;
                const float* bufferNormals = nullptr;
                const float* bufferTexCoords = nullptr;
                const float* bufferColors = nullptr;
                const float* bufferTangents = nullptr;
                uint32_t numColorComponents;
                const uint16_t* bufferJoints = nullptr;
                const float* bufferWeights = nullptr;

                // Position attribute is required
                assert(primitive.attributes.find("POSITION") != primitive.attributes.end());

                const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.find("POSITION")->second];
                const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];
                bufferPos = reinterpret_cast<const float*>(&(model.buffers[posView.buffer].data[posAccessor.byteOffset + posView.byteOffset]));
                posMin = glm::vec3(posAccessor.minValues[0], posAccessor.minValues[1], posAccessor.minValues[2]);
                posMax = glm::vec3(posAccessor.maxValues[0], posAccessor.maxValues[1], posAccessor.maxValues[2]);

                if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
                    const tinygltf::Accessor& normAccessor = model.accessors[primitive.attributes.find("NORMAL")->second];
                    const tinygltf::BufferView& normView = model.bufferViews[normAccessor.bufferView];
                    bufferNormals = reinterpret_cast<const float*>(&(model.buffers[normView.buffer].data[normAccessor.byteOffset + normView.byteOffset]));
                }

                if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
                    const tinygltf::Accessor& uvAccessor = model.accessors[primitive.attributes.find("TEXCOORD_0")->second];
                    const tinygltf::BufferView& uvView = model.bufferViews[uvAccessor.bufferView];
                    bufferTexCoords = reinterpret_cast<const float*>(&(model.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]));
                }

                if (primitive.attributes.find("COLOR_0") != primitive.attributes.end())
                {
                    const tinygltf::Accessor& colorAccessor = model.accessors[primitive.attributes.find("COLOR_0")->second];
                    const tinygltf::BufferView& colorView = model.bufferViews[colorAccessor.bufferView];
                    // Color buffer are either of type vec3 or vec4
                    numColorComponents = colorAccessor.type == TINYGLTF_PARAMETER_TYPE_FLOAT_VEC3 ? 3 : 4;
                    bufferColors = reinterpret_cast<const float*>(&(model.buffers[colorView.buffer].data[colorAccessor.byteOffset + colorView.byteOffset]));
                }

                if (primitive.attributes.find("TANGENT") != primitive.attributes.end())
                {
                    const tinygltf::Accessor& tangentAccessor = model.accessors[primitive.attributes.find("TANGENT")->second];
                    const tinygltf::BufferView& tangentView = model.bufferViews[tangentAccessor.bufferView];
                    bufferTangents = reinterpret_cast<const float*>(&(model.buffers[tangentView.buffer].data[tangentAccessor.byteOffset + tangentView.byteOffset]));
                }

                // Skinning
                // Joints
                if (primitive.attributes.find("JOINTS_0") != primitive.attributes.end()) {
                    const tinygltf::Accessor& jointAccessor = model.accessors[primitive.attributes.find("JOINTS_0")->second];
                    const tinygltf::BufferView& jointView = model.bufferViews[jointAccessor.bufferView];
                    bufferJoints = reinterpret_cast<const uint16_t*>(&(model.buffers[jointView.buffer].data[jointAccessor.byteOffset + jointView.byteOffset]));
                }

                if (primitive.attributes.find("WEIGHTS_0") != primitive.attributes.end()) {
                    const tinygltf::Accessor& uvAccessor = model.accessors[primitive.attributes.find("WEIGHTS_0")->second];
                    const tinygltf::BufferView& uvView = model.bufferViews[uvAccessor.bufferView];
                    bufferWeights = reinterpret_cast<const float*>(&(model.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]));
                }

                hasSkin = (bufferJoints && bufferWeights);

                vertexCount = static_cast<uint32_t>(posAccessor.count);

                for (size_t v = 0; v < posAccessor.count; v++) {
                    Vertex vert{};
                    vert.pos = glm::vec4(glm::make_vec3(&bufferPos[v * 3]), 1.0f);
                    vert.normal = glm::normalize(glm::vec3(bufferNormals ? glm::make_vec3(&bufferNormals[v * 3]) : glm::vec3(0.0f)));
                    vert.uv = bufferTexCoords ? glm::make_vec2(&bufferTexCoords[v * 2]) : glm::vec3(0.0f);
                    if (bufferColors) {
                        switch (numColorComponents) {
                        case 3:
                            vert.color = glm::vec4(glm::make_vec3(&bufferColors[v * 3]), 1.0f);
                            break;
                        case 4:
                            vert.color = glm::make_vec4(&bufferColors[v * 4]);
                            break;
                        }
                    }
                    else {
                        vert.color = glm::vec4(1.0f);
                    }
                    vert.tangent = bufferTangents ? glm::vec4(glm::make_vec4(&bufferTangents[v * 4])) : glm::vec4(0.0f);
                    vert.joint0 = hasSkin ? glm::vec4(glm::make_vec4(&bufferJoints[v * 4])) : glm::vec4(0.0f);
                    vert.weight0 = hasSkin ? glm::make_vec4(&bufferWeights[v * 4]) : glm::vec4(0.0f);
                    vertexBuffer.push_back(vert);
                }
            }
            // Indices
            {
                const tinygltf::Accessor& accessor = model.accessors[primitive.indices];
                const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

                indexCount = static_cast<uint32_t>(accessor.count);

                switch (accessor.componentType) {
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
                    uint32_t* buf = new uint32_t[accessor.count];
                    memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint32_t));
                    for (size_t index = 0; index < accessor.count; index++) {
                        indexBuffer.push_back(buf[index] + vertexStart);
                    }
                    delete[] buf;
                    break;
                }
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
                    uint16_t* buf = new uint16_t[accessor.count];
                    memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint16_t));
                    for (size_t index = 0; index < accessor.count; index++) {
                        indexBuffer.push_back(buf[index] + vertexStart);
                    }
                    delete[] buf;
                    break;
                }
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
                    uint8_t* buf = new uint8_t[accessor.count];
                    memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint8_t));
                    for (size_t index = 0; index < accessor.count; index++) {
                        indexBuffer.push_back(buf[index] + vertexStart);
                    }
                    delete[] buf;
                    break;
                }
                default:
                    std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
                    return;
                }
            }
            Primitive* newPrimitive = new Primitive(indexStart, indexCount, primitive.material > -1 ? materials[primitive.material] : materials.back());
            newPrimitive->firstVertex = vertexStart;
            newPrimitive->vertexCount = vertexCount;
            newMesh->primitives.push_back(newPrimitive);
        }
        newNode->mesh = newMesh;
    }
    if (parent) {
        parent->children.push_back(newNode);
    }
    else {
        nodes.push_back(newNode);
    }
    linearNodes.push_back(newNode);
}

void vulkanglTF::Model::loadFromFile(std::string filePath, uint32_t fileLoadingFlags, VkQueue transferQueue, VkCommandPool transferCommandPool, float globalScale)
{
    tinygltf::Model gltfModel;
    tinygltf::TinyGLTF gltfLoader;

    std::string error, warning;
    bool fileLoaded = gltfLoader.LoadASCIIFromFile(&gltfModel, &error, &warning, filePath);

    if (!fileLoaded) {
        throw MakeErrorInfo("glTF: Failed to load model from file!\n"
                            "Error:" + error);
    }

    if (!(fileLoadingFlags & FileLoadingFlags::DontLoadImages)) {
        loadImages(gltfModel, vulkanDevice, transferQueue);
    }
    loadMaterials(gltfModel);
    const tinygltf::Scene& scene = gltfModel.scenes[0];
    for (size_t i = 0; i < scene.nodes.size(); i++) {
        const tinygltf::Node node = gltfModel.nodes[scene.nodes[i]];
        loadNode(nullptr, node, scene.nodes[i], gltfModel, indexes, vertexes, globalScale);
    }

    for (auto node : linearNodes) {
        // Initial pose
        if (node->mesh) {
            node->update();
        }
    }

    // Pre-Calculations for requested features
    if ((fileLoadingFlags & FileLoadingFlags::PreTransformVertices) || (fileLoadingFlags & FileLoadingFlags::PreMultiplyVertexColors) || (fileLoadingFlags & FileLoadingFlags::FlipY)) {
        const bool preTransform = fileLoadingFlags & FileLoadingFlags::PreTransformVertices;
        const bool preMultiplyColor = fileLoadingFlags & FileLoadingFlags::PreMultiplyVertexColors;
        const bool flipX = fileLoadingFlags & FileLoadingFlags::FlipX;
        const bool flipY = fileLoadingFlags & FileLoadingFlags::FlipY;
        const bool flipZ = fileLoadingFlags & FileLoadingFlags::FlipZ;
        for (Node* node : linearNodes) {
            if (node->mesh) {
                const glm::mat4 localMatrix = node->getMatrix();
                for (Primitive* primitive : node->mesh->primitives) {
                    for (uint32_t i = 0; i < primitive->vertexCount; i++) {
                        Vertex& vertex = vertexes[primitive->firstVertex + i];
                        // Pre-transform vertex positions by node-hierarchy
                        if (preTransform) {
                            vertex.pos = glm::vec3(localMatrix * glm::vec4(vertex.pos, 1.0f));
                            vertex.normal = glm::normalize(glm::mat3(localMatrix) * vertex.normal);
                        }
                        // Flip X-Axis of vertex positions
                        if (flipX) {
                            vertex.pos.x *= -1.0f;
                            vertex.normal.x *= -1.0f;
                        }
                        // Flip Y-Axis of vertex positions
                        if (flipY) {
                            vertex.pos.y *= -1.0f;
                            vertex.normal.y *= -1.0f;
                        }
                        // Flip Z-Axis of vertex positions
                        if (flipZ) {
                            vertex.pos.z *= -1.0f;
                            vertex.normal.z *= -1.0f;
                        }
                        // Pre-Multiply vertex colors with material base color
                        if (preMultiplyColor) {
                            vertex.color = primitive->material.baseColorFactor * vertex.color;
                        }
                    }
                }
            }
        }
    }

    // Create and upload vertex and index buffer
    // We will be using one single vertex buffer and one single index buffer for the whole glTF scene
    // Primitives (of the glTF model) will then index into these using index offsets

    size_t vertexBufferSize = vertexes.size() * sizeof(vulkanglTF::Vertex);
    size_t indexBufferSize = indexes.size() * sizeof(uint32_t);
    vertexBuffer.count = static_cast<uint32_t>(vertexes.size());
    indexBuffer.count = static_cast<uint32_t>(indexes.size());
    vertexBuffer.vulkanBuffer = new VulkanBuffer(vulkanDevice, vmaAllocator);
    indexBuffer.vulkanBuffer = new VulkanBuffer(vulkanDevice, vmaAllocator);
    vertexBuffer.vulkanBuffer->createBuffer(
        vertexBufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    indexBuffer.vulkanBuffer->createBuffer(
        indexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    // Create staging buffers
    VulkanBuffer vertexStaging(vulkanDevice, vmaAllocator);
    VulkanBuffer indexStaging(vulkanDevice, vmaAllocator);
    vertexStaging.createBuffer(
        vertexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        vertexes.data(),
        vertexBufferSize
    );
    indexStaging.createBuffer(
        indexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        indexes.data(),
        indexBufferSize
    );

    // Copy data from staging buffer to GPU memory
    VkCommandBuffer copyCommandBuffer = vulkanDevice->beginSingleTimeCommands(transferCommandPool);
    VkBufferCopy copyRegion{};
    copyRegion.size = vertexBufferSize;
    vkCmdCopyBuffer(
        copyCommandBuffer,
        vertexStaging.buffer,
        vertexBuffer.vulkanBuffer->buffer,
        1,
        &copyRegion
    );

    copyRegion.size = indexBufferSize;
    vkCmdCopyBuffer(
        copyCommandBuffer,
        indexStaging.buffer,
        indexBuffer.vulkanBuffer->buffer,
        1,
        &copyRegion
    );

    vulkanDevice->endSingleTimeCommands(copyCommandBuffer, transferQueue, transferCommandPool);
    vertexStaging.destroy();
    indexStaging.destroy();

    // Setup descriptors
    uint32_t uboCount{ 0 };
    uint32_t imageCount{ 0 };
    for (auto& node : linearNodes) {
        if (node->mesh) {
            uboCount++;
        }
    }
    for (auto& material : materials) {
        if (material.baseColorTexture != nullptr) {
            imageCount++;
        }
    }
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uboCount },
    };
    if (imageCount > 0) {
        if (descriptorBindingFlags & DescriptorBindingFlags::ImageBaseColor) {
            poolSizes.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageCount });
        }
        if (descriptorBindingFlags & DescriptorBindingFlags::ImageNormalMap) {
            poolSizes.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageCount });
        }
    }
    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();
    descriptorPoolCreateInfo.maxSets = uboCount + imageCount;
    if (vkCreateDescriptorPool(vulkanDevice->logicalDevice, &descriptorPoolCreateInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to create command pool!");
    }

    // Descriptors for per-material images
    {
        // Layout is global, so only create if it hasn't already been created before
        if (descriptorSetLayoutImage == VK_NULL_HANDLE) {
            std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
            if (descriptorBindingFlags & DescriptorBindingFlags::ImageBaseColor) {
                setLayoutBindings.push_back(vulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, static_cast<uint32_t>(setLayoutBindings.size())));
            }
            if (descriptorBindingFlags & DescriptorBindingFlags::ImageNormalMap) {
                setLayoutBindings.push_back(vulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, static_cast<uint32_t>(setLayoutBindings.size())));
            }
            VkDescriptorSetLayoutCreateInfo descriptorLayoutCreateInfo{};
            descriptorLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptorLayoutCreateInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
            descriptorLayoutCreateInfo.pBindings = setLayoutBindings.data();
            VK_CHECK_RESULT(vkCreateDescriptorSetLayout(vulkanDevice->logicalDevice, &descriptorLayoutCreateInfo, nullptr, &descriptorSetLayoutImage));
        }
        for (auto& material : materials) {
            if (material.baseColorTexture != nullptr) {
                material.createDescriptorSet(descriptorPool, descriptorSetLayoutImage, descriptorBindingFlags);
            }
        }
    }
}

VkVertexInputBindingDescription vulkanglTF::Vertex::vertexInputBindingDescription;
std::vector<VkVertexInputAttributeDescription> vulkanglTF::Vertex::vertexInputAttributeDescriptions;
VkPipelineVertexInputStateCreateInfo vulkanglTF::Vertex::pipelineVertexInputStateCreateInfo;

VkVertexInputBindingDescription vulkanglTF::Vertex::inputBindingDescription(uint32_t binding)
{
    return VkVertexInputBindingDescription({ binding, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX });
}

VkVertexInputAttributeDescription vulkanglTF::Vertex::inputAttributeDescription(uint32_t binding, uint32_t location, VertexComponent component)
{
    switch (component) {
    case VertexComponent::Position:
        return VkVertexInputAttributeDescription({ location, binding, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos) });
    case VertexComponent::Normal:
        return VkVertexInputAttributeDescription({ location, binding, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal) });
    case VertexComponent::UV:
        return VkVertexInputAttributeDescription({ location, binding, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv) });
    case VertexComponent::Color:
        return VkVertexInputAttributeDescription({ location, binding, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, color) });
    case VertexComponent::Tangent:
        return VkVertexInputAttributeDescription({ location, binding, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, tangent) });
    case VertexComponent::Joint0:
        return VkVertexInputAttributeDescription({ location, binding, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, joint0) });
    case VertexComponent::Weight0:
        return VkVertexInputAttributeDescription({ location, binding, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, weight0) });
    default:
        return VkVertexInputAttributeDescription({});
    }
}

std::vector<VkVertexInputAttributeDescription> vulkanglTF::Vertex::inputAttributeDescriptions(uint32_t binding, const std::vector<VertexComponent> components)
{
    std::vector<VkVertexInputAttributeDescription> result;
    uint32_t location = 0;
    for (VertexComponent component : components) {
        result.push_back(Vertex::inputAttributeDescription(binding, location, component));
        location++;
    }
    return result;
}

// Get the default pipeline vertex input state create info structure for the requested vertex components
VkPipelineVertexInputStateCreateInfo* vulkanglTF::Vertex::getPipelineVertexInputState(const std::vector<VertexComponent> components)
{
    vertexInputBindingDescription = Vertex::inputBindingDescription(0);
    Vertex::vertexInputAttributeDescriptions = Vertex::inputAttributeDescriptions(0, components);
    pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
    pipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = &Vertex::vertexInputBindingDescription;
    pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(Vertex::vertexInputAttributeDescriptions.size());
    pipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = Vertex::vertexInputAttributeDescriptions.data();
    return &pipelineVertexInputStateCreateInfo;
}

void vulkanglTF::Model::bindBuffers(VkCommandBuffer commandBuffer)
{
    const VkDeviceSize offsets[1] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer.vulkanBuffer->buffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer.vulkanBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
    buffersBound = true;
}

void vulkanglTF::Model::drawNode(Node* node, VkCommandBuffer commandBuffer, uint32_t renderFlags, VkPipelineLayout pipelineLayout, uint32_t bindImageSet)
{
    if (node->mesh) {
        for (Primitive* primitive : node->mesh->primitives) {
            bool skip = false;
            const vulkanglTF::Material& material = primitive->material;
            if (renderFlags & RenderFlags::RenderOpaqueNodes) {
                skip = (material.alphaMode != Material::ALPHAMODE_OPAQUE);
            }
            if (renderFlags & RenderFlags::RenderAlphaMaskedNodes) {
                skip = (material.alphaMode != Material::ALPHAMODE_MASK);
            }
            if (renderFlags & RenderFlags::RenderAlphaBlendedNodes) {
                skip = (material.alphaMode != Material::ALPHAMODE_BLEND);
            }
            if (!skip) {
                if (renderFlags & RenderFlags::BindImages) {
                    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, bindImageSet, 1, &material.descriptorSet, 0, nullptr);
                }
                vkCmdDrawIndexed(commandBuffer, primitive->indexCount, 1, primitive->firstIndex, 0, 0);
            }
        }
    }
    for (auto& child : node->children) {
        drawNode(child, commandBuffer, renderFlags, pipelineLayout, bindImageSet);
    }
}

void vulkanglTF::Model::draw(VkCommandBuffer commandBuffer, uint32_t renderFlags, VkPipelineLayout pipelineLayout, uint32_t bindImageSet)
{
    if (!buffersBound) {
        const VkDeviceSize offsets[1] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer.vulkanBuffer->buffer, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer.vulkanBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
    }
    for (auto& node : nodes) {
        drawNode(node, commandBuffer, renderFlags, pipelineLayout, bindImageSet);
    }
}
