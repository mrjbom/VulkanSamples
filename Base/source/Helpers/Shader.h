#pragma once

#include <vulkan/vulkan.h>
#include <string>

void createShaderModuleFromSPV(VkDevice logicalDevice, std::string sourceSPVFilePath, VkShaderModule* shaderModule);

VkPipelineShaderStageCreateInfo createShaderStage(VkShaderModule shaderModule, VkShaderStageFlagBits stage);
