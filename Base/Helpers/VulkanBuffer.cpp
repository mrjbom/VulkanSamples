#include "VulkanBuffer.h"

VulkanBuffer::VulkanBuffer(VulkanDevice* vulkanDevice, VmaAllocator vmaAllocator)
{
    this->vulkanDevice = vulkanDevice;
    this->vmaAllocator = vmaAllocator;
}

VulkanBuffer::VulkanBuffer()
{
    this->vulkanDevice = nullptr;
    this->vmaAllocator = 0;
}

void VulkanBuffer::setDeviceAndAllocator(VulkanDevice* vulkanDevice, VmaAllocator vmaAllocator)
{
    this->vulkanDevice = vulkanDevice;
    this->vmaAllocator = vmaAllocator;
}

void VulkanBuffer::createBuffer(size_t bufferSize, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags requiredMemoryFlags, VmaAllocationCreateFlags vmaAllocationCreateFlags, void* pData, size_t pDataSize, VmaMemoryUsage vmaMemoryUsage)
{
    if (buffer) {
        throw MakeErrorInfo("Trying to create the same buffer multiple times!");
    }

    VkBufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = bufferSize;
    bufferCreateInfo.usage = usageFlags;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo bufferAllocationCreateInfo{};
    bufferAllocationCreateInfo.usage = vmaMemoryUsage;
    bufferAllocationCreateInfo.requiredFlags = requiredMemoryFlags;
    bufferAllocationCreateInfo.flags = vmaAllocationCreateFlags;

    if (vmaCreateBuffer(vmaAllocator, &bufferCreateInfo, &bufferAllocationCreateInfo, &buffer, &vmaAllocation, &vmaAllocationInfo) != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to create buffer!");
    }

    // Fill buffer by pData data
    if (pData) {
        void* pMappedBuffer = nullptr;
        if (vmaMapMemory(vmaAllocator, vmaAllocation, &pMappedBuffer) != VK_SUCCESS) {
            throw MakeErrorInfo("Failed to map VMA memory!");
        }
        memcpy(pMappedBuffer, pData, pDataSize);
        vmaUnmapMemory(vmaAllocator, vmaAllocation);
    }

    // It's non coherent memory
    // Need flush
    if (!(requiredMemoryFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        vmaFlushAllocation(vmaAllocator, vmaAllocation, 0, pDataSize);
    }

    this->setupDescriptor();
}

void VulkanBuffer::map(void** pMappedBuffer)
{
    if (vmaMapMemory(vmaAllocator, vmaAllocation, pMappedBuffer) != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to map VMA memory!");
    }
}

void VulkanBuffer::unmap()
{
    vmaUnmapMemory(vmaAllocator, vmaAllocation);
}

void VulkanBuffer::flush(VkDeviceSize offset, VkDeviceSize size)
{
    vmaFlushAllocation(vmaAllocator, vmaAllocation, offset, size);
}

void VulkanBuffer::setupDescriptor(VkDeviceSize size, VkDeviceSize offset)
{
    descriptor.offset = offset;
    descriptor.buffer = this->buffer;
    descriptor.range = size;
}

void VulkanBuffer::destroy()
{
    if (buffer) {
        vmaDestroyBuffer(vmaAllocator, buffer, vmaAllocation);
    }
    buffer = VK_NULL_HANDLE;
}
