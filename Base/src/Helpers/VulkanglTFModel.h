/*
 * Copyright (C) 2020 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#pragma once

#include <stdlib.h>
#include <string>
#include <fstream>
#include <vector>

#include "vulkan/vulkan.h"
#include "VulkanDevice.h"
#include "VulkanBuffer.h"
#include "VulkanTexture.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

namespace vulkanglTF
{
    enum class VertexComponent { Position, Normal, UV, Color, Tangent, Joint0, Weight0 };
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec2 uv;
        glm::vec4 color;
        glm::vec4 joint0;
        glm::vec4 weight0;
        glm::vec4 tangent;
        static VkVertexInputBindingDescription vertexInputBindingDescription;
        static std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescriptions;
        static VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo;
        static VkVertexInputBindingDescription inputBindingDescription(uint32_t binding);
        static VkVertexInputAttributeDescription inputAttributeDescription(uint32_t binding, uint32_t location, VertexComponent component);
        static std::vector<VkVertexInputAttributeDescription> inputAttributeDescriptions(uint32_t binding, const std::vector<VertexComponent> components);
        static VkPipelineVertexInputStateCreateInfo* getPipelineVertexInputState(const std::vector<VertexComponent> components);
    };

    // A primitive contains the data for a single draw call
    struct Primitive {
        uint32_t firstIndex;
        uint32_t indexCount;
        int32_t materialIndex;
    };

    // Contains the node's (optional) geometry and can be made up of an arbitrary number of primitives
    struct Mesh {
        std::vector<Primitive> primitives;
    };

    // A glTF material stores information in e.g. the texture that is attached to it and colors
    struct Material {
        glm::vec4 baseColorFactor = glm::vec4(1.0f);
        uint32_t baseColorTextureIndex;
    };

    // Contains the texture for a single glTF image
    // Images may be reused by texture objects and are as such separated
    struct Image {
        VulkanTexture2D texture;
        // We also store (and create) a descriptor set that's used to access this texture from the fragment shader
        VkDescriptorSet descriptorSet;
    };

    // A glTF texture stores a reference to the image and a sampler
    struct Texture {
        int32_t imageIndex;
    };

    // A node represents an object in the glTF scene graph
    struct Node {
        Node* parent;
        std::vector<Node> childrens;
        Mesh mesh;
        glm::mat4 matrix;
    };

    class Model
    {
        // Single vertex buffer for all primitives
        struct {
            int count = 0;
            VulkanBuffer* vulkanBuffer = nullptr;
        } vertexBuffer;

        // Single index buffer for all primitives
        struct {
            int count = 0;
            VulkanBuffer* vulkanBuffer = nullptr;
        } indexBuffer;
    public:
        VulkanDevice* vulkanDevice = nullptr;
        VkQueue transferQueue;
        VkCommandPool transferCommandPool = VK_NULL_HANDLE;
        VmaAllocator vmaAllocator = 0;

        std::vector<Image> images;
        std::vector<Material> materials;
        std::vector<Texture> textures;
        // Scene nodes (top levels nodes)
        std::vector<Node> nodes;

        bool buffersBound = false;

        std::vector<uint32_t> indexes;
        std::vector<Vertex> vertexes;

        VkVertexInputBindingDescription vertexInputBindingDescription;
        std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescriptions;
        VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo;
    public:
        Model(VulkanDevice* vulkanDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, VmaAllocator vmaAllocator);
        ~Model();
        void loadImages(tinygltf::Model& gltfModel);
        void loadMaterials(tinygltf::Model& gltfModel);
        void loadTextures(tinygltf::Model& gltfModel);
        void loadNode(const tinygltf::Node& inputNode, const tinygltf::Model& gltfModel, vulkanglTF::Node* parent, std::vector<Vertex>& vertexec, std::vector<uint32_t>& indexec);
        void loadFromFile(std::string filePath, VkQueue transferQueue, VkCommandPool transferCommandPool);
        void bindBuffers(VkCommandBuffer commandBuffer);
        void drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, Node node);
        void draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout);
    };
}
