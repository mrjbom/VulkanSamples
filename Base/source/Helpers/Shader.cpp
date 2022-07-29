#include "Shader.h"
#include "../ErrorInfo/ErrorInfo.h"
#include <fstream>
#include <vector>

void createShaderModuleFromSPV(VkDevice logicalDevice, std::string sourceSPVFilePath, VkShaderModule* shaderModule)
{
    std::ifstream sourceFile(sourceSPVFilePath, std::ios::ate | std::ios::binary);
    if (!sourceFile.is_open()) {
        throw MakeErrorInfo("Failed to open shader source file:\n" + sourceSPVFilePath);
    }
    size_t fileSize = (size_t)sourceFile.tellg();
    std::vector<char> sourceSPVBinary(fileSize);
    sourceFile.seekg(0);
    sourceFile.read(sourceSPVBinary.data(), fileSize);
    sourceFile.close();

    // Create shader module
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = sourceSPVBinary.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(sourceSPVBinary.data());

    if (vkCreateShaderModule(logicalDevice, &createInfo, nullptr, shaderModule) != VK_SUCCESS) {
        throw MakeErrorInfo("Failed to create shader module for: \n" + sourceSPVFilePath);
    }
}

VkPipelineShaderStageCreateInfo createShaderStage(VkShaderModule shaderModule, VkShaderStageFlagBits stage)
{
    VkPipelineShaderStageCreateInfo shaderStaheCreateInfo{};
    shaderStaheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStaheCreateInfo.stage = stage;
    shaderStaheCreateInfo.module = shaderModule;
    shaderStaheCreateInfo.pName = "main";

    return shaderStaheCreateInfo;
}
