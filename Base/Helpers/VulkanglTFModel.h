/*
 * Copyright (C) 2020 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

 // TODO
 // Need to think about the possibility of using additional textures of materials.
 // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#additional-textures

#pragma once

#include <stdlib.h>
#include <string>
#include <fstream>
#include <vector>

#include "vulkan/vulkan.h"
#include "VulkanDevice.h"
#include "VulkanBuffer.h"
#include "VulkanTexture.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace vulkanglTF
{
    enum FileLoadingFlags {
        None = 0x00000000,
        PreTransformVertices = 0x00000001,
        PreMultiplyVertexColors = 0x00000002,
        FlipX = 0x00000004,
        FlipY = 0x00000008,
        FlipZ = 0x00000010,
        DontLoadImages = 0x000000020
    };

    enum DescriptorBindingFlags {
        ImageBaseColor = 0x00000001,
        ImageNormalMap = 0x00000002
    };

    enum RenderFlags {
        BindImages = 0x00000001,
        RenderOpaqueNodes = 0x00000002,
        RenderAlphaMaskedNodes = 0x00000004,
        RenderAlphaBlendedNodes = 0x00000008
    };

    extern uint32_t descriptorBindingFlags;
    extern VkDescriptorSetLayout descriptorSetLayoutImage;

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

    struct Material;

    // A primitive contains the data for a single draw call
    struct Primitive {
        uint32_t firstIndex;
        uint32_t indexCount;
        uint32_t firstVertex;
        uint32_t vertexCount;
        Material& material;

        Primitive(uint32_t firstIndex, uint32_t indexCount, Material& material) : firstIndex(firstIndex), indexCount(indexCount), material(material) {};
    };

    // Contains the node's (optional) geometry and can be made up of an arbitrary number of primitives
    struct Mesh {
        VulkanDevice* vulkanDevice;
        VmaAllocator vmaAllocator;

        std::vector<Primitive*> primitives;
        std::string name;

        struct UniformBuffer {
            VulkanBuffer* vulkanBuffer;
            VkDescriptorBufferInfo descriptor;
            VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        } uniformBuffer;

        struct UniformBlock {
            glm::mat4 matrix;
        } uniformBlock;

        Mesh(VulkanDevice* vulkanDevice, VmaAllocator vmaAllocator, glm::mat4 matrix);
        ~Mesh();
    };

    // A glTF material stores information in e.g. the texture that is attached to it and colors
    struct Material {
        VulkanDevice* vulkanDevice = nullptr;
        enum AlphaMode { ALPHAMODE_OPAQUE, ALPHAMODE_MASK, ALPHAMODE_BLEND };
        AlphaMode alphaMode = ALPHAMODE_OPAQUE;
        float alphaCutoff = 1.0f;
        float metallicFactor = 1.0f;
        float roughnessFactor = 1.0f;
        glm::vec4 baseColorFactor = glm::vec4(1.0f);
        VulkanTexture2D* baseColorTexture = nullptr;
        VulkanTexture2D* metallicRoughnessTexture = nullptr;
        VulkanTexture2D* normalTexture = nullptr;
        VulkanTexture2D* occlusionTexture = nullptr;
        VulkanTexture2D* emissiveTexture = nullptr;

        VulkanTexture2D* specularGlossinessTexture;
        VulkanTexture2D* diffuseTexture;

        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

        Material(VulkanDevice* vulkanDevice) : vulkanDevice(vulkanDevice) {};
        void createDescriptorSet(VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout, uint32_t descriptorBindingFlags);
    };

    // Contains the texture for a single glTF image
    // Images may be reused by texture objects and are as such separated
    struct Image {
        VulkanTexture2D texture;
        // We also store (and create) a descriptor set that's used to access this texture from the fragment shader
        VkDescriptorSet descriptorSet;
    };

    // A node represents an object in the glTF scene graph
    struct Node {
        Node* parent;
        uint32_t index;
        std::vector<Node*> children;
        glm::mat4 matrix{ 1.0f };
        std::string name;
        Mesh* mesh;
        glm::vec3 translation{};
        glm::vec3 scale{ 1.0f };
        glm::quat rotation{};
        glm::mat4 localMatrix();
        glm::mat4 getMatrix();
        void update();
        ~Node();
    };

    class Model
    {
    private:
        VulkanTexture2D* getTexture(uint32_t index);
        VulkanTexture2D emptyTexture;
        void createEmptyTexture(VkQueue transferQueue);
    public:
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
        VmaAllocator vmaAllocator = 0;
        VkQueue transferQueue;
        VkCommandPool transferCommandPool = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

        std::vector<Material> materials;
        std::vector<VulkanTexture2D> textures;

        std::string path;

        std::vector<Node*> nodes;
        std::vector<Node*> linearNodes;

        bool buffersBound = false;

        std::vector<uint32_t> indexes;
        std::vector<Vertex> vertexes;

        VkVertexInputBindingDescription vertexInputBindingDescription;
        std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescriptions;
        VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo;
    public:
        Model(VulkanDevice* vulkanDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, VmaAllocator vmaAllocator);
        ~Model();
        void loadImages(tinygltf::Model& gltfModel, VulkanDevice* device, VkQueue transferQueue);
        void loadMaterials(tinygltf::Model& gltfModel);
        void loadNode(Node* parent, const tinygltf::Node& node, uint32_t nodeIndex, const tinygltf::Model& model, std::vector<uint32_t>& indexBuffer, std::vector<Vertex>& vertexBuffer, float globalScale = 1.0f);
        void loadFromFile(std::string filePath, uint32_t fileLoadingFlags, VkQueue transferQueue, VkCommandPool transferCommandPool, float globalScale = 1.0f);
        void bindBuffers(VkCommandBuffer commandBuffer);
        void drawNode(Node* node, VkCommandBuffer commandBuffer, uint32_t renderFlags = 0, VkPipelineLayout pipelineLayout = VK_NULL_HANDLE, uint32_t bindImageSet = 1);
        void draw(VkCommandBuffer commandBuffer, uint32_t renderFlags = 0, VkPipelineLayout pipelineLayout = VK_NULL_HANDLE, uint32_t bindImageSet = 1);
    };
}