#pragma once

#include <vulkan/vulkan.h>
#include "VulkanDevice.h"
#include "VulkanTools.h"
#include "vk_mem_alloc.h"

class VulkanBuffer
{
private:
    VulkanDevice* vulkanDevice = nullptr;
    VmaAllocator vmaAllocator = NULL;

public:
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation vmaAllocation{};
    VmaAllocationInfo vmaAllocationInfo{};
public:
    VulkanBuffer(VulkanDevice* vulkanDevice, VmaAllocator vmaAllocator);
    
    // Creating buffer using VMA
    // - bufferSize
    // The size of the new buffer
    // - usageFlags
    // Something like VK_BUFFER_USAGE_TRANSFER_SRC_BIT
    // - requiredMemoryFlags
    // Something like VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    // - vmaAllocationCreateFlags
    // Something like VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
    // Default: 0
    // - pData
    // If not nullptr, it sets a pointer to the data that must be copied to the buffer after it is created
    // - pDataSize
    // The size of the data that must be copied from pData
    // - vmaMemoryUsage
    // Default: VMA_MEMORY_USAGE_AUTO
    void createBuffer(
        size_t bufferSize,
        VkBufferUsageFlags usageFlags,
        VkMemoryPropertyFlags requiredMemoryFlags,
        VmaAllocationCreateFlags vmaAllocationCreateFlags = 0,
        void* pData = nullptr,
        size_t pDataSize = 0,
        VmaMemoryUsage vmaMemoryUsage = VMA_MEMORY_USAGE_AUTO
    );

    void map(void** pMappedBuffer);
    void unmap();
    void flush(VkDeviceSize offset, VkDeviceSize size);

    void destroy();
};
