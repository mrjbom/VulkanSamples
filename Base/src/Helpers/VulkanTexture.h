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
    VulkanDevice* vulkanDevice = nullptr;
    VkImage                 image = VK_NULL_HANDLE;
    VkImageView             imageView = VK_NULL_HANDLE;
    VkImageLayout           imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    uint32_t                width = 0, height = 0;
    uint32_t                mipLevels = 0;
    uint32_t                layerCount = 0;
    VkDescriptorImageInfo   descriptor{};
    VkSampler               sampler = VK_NULL_HANDLE;

    // For VMA
    VmaAllocation           vmaAllocation = nullptr;
    VmaAllocationInfo       vmaAllocationInfo{};
};

