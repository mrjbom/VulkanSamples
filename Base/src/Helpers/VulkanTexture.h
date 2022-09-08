#pragma once

#include <vulkan/vulkan.h>
#include "VulkanDevice.h"
#include "VulkanTools.h"
#include "vk_mem_alloc.h"
#define KHRONOS_STATIC
#include <ktx.h>

class VulkanTexture
{
public:
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
    VmaAllocation           vmaAllocation = nullptr;
    VmaAllocationInfo       vmaAllocationInfo{};
    
    void destroy(VulkanDevice* vulkanDevice, VmaAllocator vmaAllocator);
};

class VulkanTexture2D : public VulkanTexture
{
public:
    // Load image data from byte array(Raw data of RGBA image!)
    // Create VkImage, VkImageView and VkSampler for texture
    void createTextureFromRawData(
        VulkanDevice* vulkanDevice,
        VkQueue transferQueue,
        VkCommandPool transferCommandPool,
        unsigned char* imageData,
        uint32_t width,
        uint32_t height,
        VmaAllocator vmaAllocator,
        VkFilter filter = VK_FILTER_LINEAR,
        VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
        VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
};

