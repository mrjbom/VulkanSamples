#pragma once

#include <vulkan/vulkan.h>
#include "VulkanDevice.h"
#include "VulkanTools.h"
#include "VulkanBuffer.h"
#include "vk_mem_alloc.h"
#define KHRONOS_STATIC
#include <ktx.h>

class VulkanTexture
{
public:
    VulkanDevice*           vulkanDevice = nullptr;
    VmaAllocator            vmaAllocator = 0;
    VkImage                 image = VK_NULL_HANDLE;
    VkImageView             imageView = VK_NULL_HANDLE;
    VkImageLayout           imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    uint32_t                width = 0, height = 0;
    uint32_t                mipLevels = 1;
    uint32_t                layerCount = 1;
    uint32_t                facesCount = 1;
    VkFormat                textureFormat = VK_FORMAT_R8G8B8A8_UNORM; // 4 channels (RGBA) with unnormalized 8-bit values, this is the most commonly supported format
    VkDescriptorImageInfo   descriptor{};
    VkSampler               sampler = VK_NULL_HANDLE;

    // For VMA
    VmaAllocation           vmaImageAllocation = nullptr;
    VmaAllocationInfo       vmaImageAllocationInfo{};
public:
    void updateDescriptor();

    void destroy();
};

class VulkanTexture2D : public VulkanTexture
{
public:
    VulkanTexture2D(VulkanDevice* vulkanDevice, VmaAllocator vmaAllocator);

    VulkanTexture2D();

    // Set the device and allocator if they were not specified in the constructor
    void setDeviceAndAllocator(VulkanDevice* vulkanDevice, VmaAllocator vmaAllocator);

    // Load image data from byte array(Raw data of RGBA image!)
    // Create VkImage, VkImageView and VkSampler for texture
    void createTextureFromRawData(
        VkQueue transferQueue,
        VkCommandPool transferCommandPool,
        unsigned char* imageData,
        uint32_t width,
        uint32_t height,
        VkFilter filter = VK_FILTER_LINEAR,
        VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
        VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    // Load image data from KTX texture file
    // Create VkImage, VkImageView and VkSampler for texture
    void createTextureFromKTX(
        VkQueue transferQueue,
        VkCommandPool transferCommandPool,
        std::string filePath,
        VkFilter filter = VK_FILTER_LINEAR,
        VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
        VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
};

