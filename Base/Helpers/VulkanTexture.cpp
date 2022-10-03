#include "VulkanTexture.h"

void VulkanTexture::updateDescriptor()
{
    descriptor.sampler = sampler;
    descriptor.imageView = imageView;
    descriptor.imageLayout = imageLayout;
}

void VulkanTexture::destroy()
{
    if (sampler) {
        vkDestroySampler(vulkanDevice->logicalDevice, sampler, nullptr);
    }
    if (imageView) {
        vkDestroyImageView(vulkanDevice->logicalDevice, imageView, nullptr);
    }
    if (image) {
        vmaDestroyImage(vmaAllocator, image, vmaImageAllocation);
    }
}

VulkanTexture2D::VulkanTexture2D(VulkanDevice* vulkanDevice, VmaAllocator vmaAllocator)
{
    this->vulkanDevice = vulkanDevice;
    this->vmaAllocator = vmaAllocator;
}

VulkanTexture2D::VulkanTexture2D()
{
    this->vulkanDevice = nullptr;
    this->vmaAllocator = 0;
}

void VulkanTexture2D::setDeviceAndAllocator(VulkanDevice* vulkanDevice, VmaAllocator vmaAllocator)
{
    this->vulkanDevice = vulkanDevice;
    this->vmaAllocator = vmaAllocator;
}


void VulkanTexture2D::createTextureFromRawData(VkQueue transferQueue, VkCommandPool transferCommandPool, unsigned char* imageData, uint32_t width, uint32_t height, VkFilter filter, VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout)
{
    // Load image raw data to GPU memory
    // 
    // We create the image in the local memory of the device(without the possibility of mapping to the host memory)
    // and use an staging buffer to copy the texture data to image memory

    this->width = width;
    this->height = height;
    this->mipLevels = 1;

    // RGBA
    uint32_t imageSize = width * height * 4;

    // Create staging buffer
    VulkanBuffer stagingBuffer(vulkanDevice, vmaAllocator);
    stagingBuffer.createBuffer(
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        imageData,
        imageSize
    );

    // Create image
    VkImageCreateInfo imageCreateInfo{};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = textureFormat;
    imageCreateInfo.extent = { width, height, 1 };
    imageCreateInfo.mipLevels = mipLevels;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo imageAllocationCreateInfo{};
    imageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    imageAllocationCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    if (vmaCreateImage(vmaAllocator, &imageCreateInfo, &imageAllocationCreateInfo, &image, &vmaImageAllocation, &vmaImageAllocationInfo) != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to create image!");
    }

    // Copy image data from buffer to image
    VkCommandBuffer commandBuffer = vulkanDevice->beginSingleTimeCommands(transferCommandPool);

    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = mipLevels;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;

    // Transition the texture image layout to transfer target, so we can safely copy our buffer data to it
    vulkanTools::insertImageMemoryBarrier(
        commandBuffer,
        image,
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
    bufferCopyRegion.imageExtent.width = width;
    bufferCopyRegion.imageExtent.height = height;
    bufferCopyRegion.imageExtent.depth = 1;
    bufferCopyRegion.bufferOffset = 0;

    // Copy image from staging buffer
    vkCmdCopyBufferToImage(
        commandBuffer,
        stagingBuffer.buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &bufferCopyRegion
    );

    // Change image layout to imageLayout after transfer
    this->imageLayout = imageLayout;
    vulkanTools::insertImageMemoryBarrier(
        commandBuffer,
        image,
        VK_ACCESS_TRANSFER_WRITE_BIT, // srcAccessMask - We write the data to the image
        VK_ACCESS_SHADER_READ_BIT, // dstAccessMask - The shader reads data from the image
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // oldImageLayout
        imageLayout, // newImageLayout
        VK_PIPELINE_STAGE_TRANSFER_BIT, // srcStageMask - We have to wait for the transfer operation that loads the image
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // dstStageMask - The shader should read the data only after it is loaded to image
        subresourceRange
    );

    vulkanDevice->endSingleTimeCommands(commandBuffer, transferQueue, transferCommandPool);
    // Destroy staging buffer
    stagingBuffer.destroy();

    // Create image view
    VkImageViewCreateInfo imageViewCreateInfo{};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.image = image;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = textureFormat;
    imageViewCreateInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = mipLevels;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(vulkanDevice->logicalDevice, &imageViewCreateInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to create image view!");
    }

    // Create sampler
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = filter;
    samplerCreateInfo.minFilter = filter;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = 0.0f;
    if (vulkanDevice->enabledFeatures.samplerAnisotropy == VK_TRUE) {
        samplerCreateInfo.anisotropyEnable = VK_TRUE;
        samplerCreateInfo.maxAnisotropy = vulkanDevice->properties.limits.maxSamplerAnisotropy;
    }
    else {
        samplerCreateInfo.anisotropyEnable = VK_FALSE;
        samplerCreateInfo.maxAnisotropy = 1.0f;
    }

    if (vkCreateSampler(vulkanDevice->logicalDevice, &samplerCreateInfo, nullptr, &sampler) != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to create sampler!");
    }

    descriptor.sampler = sampler;
    descriptor.imageView = imageView;
    descriptor.imageLayout = imageLayout;
}

void VulkanTexture2D::createTextureFromKTX(VkQueue transferQueue, VkCommandPool transferCommandPool, std::string filePath, VkFilter filter, VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout)
{
    // We create the image in the local memory of the device(without the possibility of mapping to the host memory)
    // and use an staging buffer to copy the texture data to image memory

    ktxResult result;
    ktxTexture* ktxTexture;

    if (!vulkanTools::fileExists(filePath)) {
        throw MakeErrorInfo("File " + filePath + " not exist! Check assets files!");
    }

    result = ktxTexture_CreateFromNamedFile(filePath.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
    if (result != KTX_SUCCESS) {
        throw MakeErrorInfo("ktx: failed to load texture!");
    }

    // Get texture properties 
    width = ktxTexture->baseWidth;
    height = ktxTexture->baseHeight;
    mipLevels = ktxTexture->numLevels;
    layerCount = ktxTexture->numLayers;
    ktx_uint8_t* ktxTextureData = ktxTexture_GetData(ktxTexture);
    ktx_size_t ktxTextureSize = ktxTexture_GetDataSize(ktxTexture);

    // Create staging buffer
    VulkanBuffer stagingBuffer(vulkanDevice, vmaAllocator);
    stagingBuffer.createBuffer(
        ktxTextureSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        ktxTextureData,
        ktxTextureSize
    );

    // Create image
    VkImageCreateInfo imageCreateInfo{};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = textureFormat;
    imageCreateInfo.extent = { width, height, 1 };
    imageCreateInfo.mipLevels = mipLevels;
    imageCreateInfo.arrayLayers = layerCount;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo imageAllocationCreateInfo{};
    imageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    imageAllocationCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    if (vmaCreateImage(vmaAllocator, &imageCreateInfo, &imageAllocationCreateInfo, &image, &vmaImageAllocation, &vmaImageAllocationInfo) != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to create image!");
    }

    // Copy data from staging buffer to image
    VkCommandBuffer commandBuffer = vulkanDevice->beginSingleTimeCommands(transferCommandPool);

    // The sub resource range describes the regions of the image that will be transitioned using the memory barriers below
    VkImageSubresourceRange subresourceRange = {};
    // Image only contains color data
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    // Start at first mip level
    subresourceRange.baseMipLevel = 0;
    // We will transition on all layers and mip levels
    subresourceRange.levelCount = mipLevels;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = layerCount;

    // Transition the texture image layout to transfer target, so we can safely copy our buffer data to it
    vulkanTools::insertImageMemoryBarrier(
        commandBuffer,
        image,
        0, // srcAccessMask - We do not perform any operations before memory barrier
        VK_ACCESS_TRANSFER_WRITE_BIT, // dstAccessMask - We write after memory barrier
        VK_IMAGE_LAYOUT_UNDEFINED, // oldImageLayout
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // newImageLayout
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // srcStageMask - We don't wait anything before the barrier
        VK_PIPELINE_STAGE_TRANSFER_BIT, // dstStageMask - Stages in which we make transfer operations should wait a barrier
        subresourceRange);

    // Copy buffer to image

    // Setup buffer copy regions for each layer and mip level
    std::vector<VkBufferImageCopy> bufferCopyRegions;
    for (uint32_t currentLayer = 0; currentLayer < layerCount; currentLayer++) {
        for (uint32_t currentLevel = 0; currentLevel < mipLevels; currentLevel++) {
            ktx_size_t offset;
            KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, currentLevel, currentLayer, 0, &offset);
            assert(ret == KTX_SUCCESS);
            // Setup a buffer image copy structure for the current layer and mip level
            VkBufferImageCopy bufferCopyRegion = {};
            bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel = currentLevel;
            bufferCopyRegion.imageSubresource.baseArrayLayer = currentLayer;
            bufferCopyRegion.imageSubresource.layerCount = 1;
            bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth >> currentLevel;
            bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight >> currentLevel;
            bufferCopyRegion.imageExtent.depth = 1;
            bufferCopyRegion.bufferOffset = offset;
            bufferCopyRegions.push_back(bufferCopyRegion);
        }
    }

    // Copy mip levels from staging buffer
    vkCmdCopyBufferToImage(
        commandBuffer,
        stagingBuffer.buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        bufferCopyRegions.size(),
        bufferCopyRegions.data());

    // Change image layout to imageLayout after transfer
    this->imageLayout = imageLayout;
    vulkanTools::insertImageMemoryBarrier(
        commandBuffer,
        image,
        VK_ACCESS_TRANSFER_WRITE_BIT, // srcAccessMask - We write the data to the image
        VK_ACCESS_SHADER_READ_BIT, // dstAccessMask - The shader reads data from the image
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // oldImageLayout
        imageLayout, // newImageLayout
        VK_PIPELINE_STAGE_TRANSFER_BIT, // srcStageMask - We have to wait for the transfer operation that loads the image
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // dstStageMask - The shader should read the data only after it is loaded to image
        subresourceRange
    );

    vulkanDevice->endSingleTimeCommands(commandBuffer, transferQueue, transferCommandPool);

    // Destroy staging buffer
    stagingBuffer.destroy();
    ktxTexture_Destroy(ktxTexture);

    // Create image view
    VkImageViewCreateInfo imageViewCreateInfo{};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.image = image;
    imageViewCreateInfo.viewType = layerCount > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = textureFormat;
    imageViewCreateInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = mipLevels;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = layerCount;

    if (vkCreateImageView(vulkanDevice->logicalDevice, &imageViewCreateInfo, nullptr, &imageView) != VK_SUCCESS) {
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
    if (vulkanDevice->enabledFeatures.samplerAnisotropy == VK_TRUE) {
        samplerCreateInfo.anisotropyEnable = VK_TRUE;
        samplerCreateInfo.maxAnisotropy = vulkanDevice->properties.limits.maxSamplerAnisotropy;
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
    samplerCreateInfo.maxLod = (float)mipLevels;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

    if (vkCreateSampler(vulkanDevice->logicalDevice, &samplerCreateInfo, nullptr, &sampler) != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to create sampler!");
    }

    descriptor.sampler = sampler;
    descriptor.imageView = imageView;
    descriptor.imageLayout = imageLayout;
}
