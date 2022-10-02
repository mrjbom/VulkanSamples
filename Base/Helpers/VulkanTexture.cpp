#include "VulkanTexture.h"

void VulkanTexture::destroy(VulkanDevice* vulkanDevice, VmaAllocator vmaAllocator)
{
    if (sampler) {
        vkDestroySampler(vulkanDevice->logicalDevice, sampler, nullptr);
    }
    if (imageView) {
        vkDestroyImageView(vulkanDevice->logicalDevice, imageView, nullptr);
    }
    if (image) {
        vmaDestroyImage(vmaAllocator, image, vmaAllocation);
    }
}

void VulkanTexture2D::createTextureFromRawData(VulkanDevice* vulkanDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, unsigned char* imageData, uint32_t width, uint32_t height, VmaAllocator vmaAllocator, VkFilter filter, VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout)
{
    // Load image raw data to GPU memory
    // 
    // We create the image in the local memory of the device(without the possibility of mapping to the host memory)
    // and use an staging buffer to copy the texture data to image memory

    this->width = width;
    this->height = height;
    this->mipLevels = 1;

    uint32_t imageSize = width * height * 4;

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
    bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    bufferAllocationCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    if (vmaCreateImage(vmaAllocator, &imageCreateInfo, &imageAllocationCreateInfo, &image, &vmaAllocation, &vmaAllocationInfo) != VK_SUCCESS) {
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
        stagingBuffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &bufferCopyRegion
    );

    // Change image layout to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL after transfer
    vulkanTools::insertImageMemoryBarrier(
        commandBuffer,
        image,
        VK_ACCESS_TRANSFER_WRITE_BIT, // srcAccessMask - We write the data to the image
        VK_ACCESS_SHADER_READ_BIT, // dstAccessMask - The shader reads data from the image
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // oldImageLayout
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, // newImageLayout
        VK_PIPELINE_STAGE_TRANSFER_BIT, // srcStageMask - We have to wait for the transfer operation that loads the image
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // dstStageMask - The shader should read the data only after it is loaded to image
        subresourceRange
    );

    // Store current layout for later reuse
    imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    vulkanDevice->endSingleTimeCommands(commandBuffer, transferQueue, transferCommandPool);
    // Destroy staging buffer
    vmaDestroyBuffer(vmaAllocator, stagingBuffer, stagingBufferAllocation);

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
    samplerCreateInfo.anisotropyEnable = VK_FALSE;
    samplerCreateInfo.maxAnisotropy = 1.0f;

    if (vkCreateSampler(vulkanDevice->logicalDevice, &samplerCreateInfo, nullptr, &sampler) != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to create sampler!");
    }

    descriptor.sampler = sampler;
    descriptor.imageView = imageView;
    descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}
