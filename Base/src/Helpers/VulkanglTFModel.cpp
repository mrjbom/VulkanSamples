/*
 * Copyright (C) 2020 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE

#include "VulkanglTFModel.h"

vulkanglTF::Model::Model(VulkanDevice* vulkanDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, VmaAllocator vmaAllocator)
{
    this->vulkanDevice = vulkanDevice;
    this->transferQueue = transferQueue;
    this->transferCommandPool = transferCommandPool;
    this->vmaAllocator = vmaAllocator;
}

vulkanglTF::Model::~Model()
{
    for (auto& image : images) {
        image.texture.destroy(vulkanDevice, vmaAllocator);
    }

    vertexBuffer.vulkanBuffer->destroy();
    indexBuffer.vulkanBuffer->destroy();
    delete vertexBuffer.vulkanBuffer;
    delete indexBuffer.vulkanBuffer;
}

void vulkanglTF::Model::loadImages(tinygltf::Model& gltfModel)
{
    images.resize(gltfModel.images.size());
    for (size_t i = 0; i < gltfModel.images.size(); i++) {
        tinygltf::Image& glTFImage = gltfModel.images[i];
        // Get the image data from the glTF loader
        uint8_t* buffer = nullptr;
        uint32_t bufferSize = 0;
        bool deleteBuffer = false;
        // We convert RGB-only images to RGBA, as most devices don't support RGB-formats in Vulkan
        if (glTFImage.component == 3) {
            bufferSize = glTFImage.width * glTFImage.height * 4;
            buffer = new uint8_t[bufferSize];
            uint8_t* rgba = buffer;
            uint8_t* rgb = &glTFImage.image[0];
            for (int i = 0; i < glTFImage.width * glTFImage.height; ++i) {
                memcpy(rgba, rgb, sizeof(uint8_t) * 3);
                rgba += 4;
                rgb += 3;
            }
            deleteBuffer = true;
        }
        else {
            buffer = &glTFImage.image[0];
            bufferSize = glTFImage.image.size();
        }
        // Load texture from image buffer
        images[i].texture.createTextureFromRawData(vulkanDevice, transferQueue, transferCommandPool, buffer, glTFImage.width, glTFImage.height, vmaAllocator, VK_FILTER_LINEAR, VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        if (deleteBuffer) {
            delete[] buffer;
        }
    }
}
void vulkanglTF::Model::loadMaterials(tinygltf::Model& gltfModel)
{
    materials.resize(gltfModel.materials.size());
    for (size_t i = 0; i < gltfModel.materials.size(); i++) {
        // We only read the most basic properties required for our sample
        tinygltf::Material glTFMaterial = gltfModel.materials[i];
        // Get the base color factor
        if (glTFMaterial.values.find("baseColorFactor") != glTFMaterial.values.end()) {
            materials[i].baseColorFactor = glm::make_vec4(glTFMaterial.values["baseColorFactor"].ColorFactor().data());
        }
        // Get base color texture index
        if (glTFMaterial.values.find("baseColorTexture") != glTFMaterial.values.end()) {
            materials[i].baseColorTextureIndex = glTFMaterial.values["baseColorTexture"].TextureIndex();
        }
    }
}

void vulkanglTF::Model::loadTextures(tinygltf::Model& gltfModel)
{
    textures.resize(gltfModel.textures.size());
    for (size_t i = 0; i < gltfModel.textures.size(); i++) {
        textures[i].imageIndex = gltfModel.textures[i].source;
    }
}

void vulkanglTF::Model::loadNode(const tinygltf::Node& inputNode, const tinygltf::Model& gltfModel, vulkanglTF::Node* parent, std::vector<Vertex>& vertexec, std::vector<uint32_t>& indexec)
{
    Node node{};
    node.matrix = glm::mat4(1.0f);

    // Get the local node matrix
    // It's either made up from translation, rotation, scale or a 4x4 matrix
    if (inputNode.translation.size() != 0) {
        node.matrix = glm::translate(node.matrix, glm::vec3(glm::make_vec3(inputNode.translation.data())));
    }
    if (inputNode.rotation.size() != 0) {
        glm::quat q = glm::make_quat(inputNode.rotation.data());
        node.matrix *= glm::mat4(q);
    }
    if (inputNode.scale.size() != 0) {
        node.matrix = glm::scale(node.matrix, glm::vec3(glm::make_vec3(inputNode.scale.data())));
    }
    if (inputNode.matrix.size() != 0) {
        node.matrix = glm::make_mat4x4(inputNode.matrix.data());
    };

    // Load current node childrens
    if (inputNode.children.size() > 0) {
        for (size_t i = 0; i < inputNode.children.size(); i++) {
            loadNode(gltfModel.nodes[inputNode.children[i]], gltfModel, &node, vertexec, indexec);
        }
    }

    // If the node contains mesh data, we load vertices and indices from the buffers
    // In glTF this is done via accessors and buffer views
    if (inputNode.mesh > -1) {
        const tinygltf::Mesh mesh = gltfModel.meshes[inputNode.mesh];
        // Iterate through all primitives of this node's mesh
        for (size_t i = 0; i < mesh.primitives.size(); i++) {
            const tinygltf::Primitive& glTFPrimitive = mesh.primitives[i];
            uint32_t firstIndex = static_cast<uint32_t>(indexec.size());
            uint32_t vertexStart = static_cast<uint32_t>(vertexec.size());
            uint32_t indexCount = 0;
            // Vertices
            {
                const float* positionBuffer = nullptr;
                const float* normalsBuffer = nullptr;
                const float* texCoordsBuffer = nullptr;
                size_t vertexCount = 0;

                // Get buffer data for vertex normals
                if (glTFPrimitive.attributes.find("POSITION") != glTFPrimitive.attributes.end()) {
                    const tinygltf::Accessor& accessor = gltfModel.accessors[glTFPrimitive.attributes.find("POSITION")->second];
                    const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
                    positionBuffer = reinterpret_cast<const float*>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                    // Number of vertexes
                    vertexCount = accessor.count;
                }
                // Get buffer data for vertex normals
                if (glTFPrimitive.attributes.find("NORMAL") != glTFPrimitive.attributes.end()) {
                    const tinygltf::Accessor& accessor = gltfModel.accessors[glTFPrimitive.attributes.find("NORMAL")->second];
                    const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
                    normalsBuffer = reinterpret_cast<const float*>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                }
                // Get buffer data for vertex texture coordinates
                // glTF supports multiple sets, we only load the first one
                if (glTFPrimitive.attributes.find("TEXCOORD_0") != glTFPrimitive.attributes.end()) {
                    const tinygltf::Accessor& accessor = gltfModel.accessors[glTFPrimitive.attributes.find("TEXCOORD_0")->second];
                    const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
                    texCoordsBuffer = reinterpret_cast<const float*>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                }

                // Append data to model's vertex buffer
                for (size_t v = 0; v < vertexCount; v++) {
                    Vertex vert{};
                    vert.pos = glm::make_vec3(&positionBuffer[v * 3]);
                    vert.normal = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
                    vert.uv = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec3(0.0f);
                    vert.color = glm::vec4(1.0f);
                    vertexec.push_back(vert);
                }
            }
            // Indices
            {
                const tinygltf::Accessor& accessor = gltfModel.accessors[glTFPrimitive.indices];
                const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = gltfModel.buffers[bufferView.buffer];

                indexCount += static_cast<uint32_t>(accessor.count);

                // glTF supports different component types of indices
                switch (accessor.componentType) {
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
                    uint32_t* buf = new uint32_t[accessor.count];
                    memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint32_t));
                    for (size_t index = 0; index < accessor.count; index++) {
                        indexes.push_back(buf[index] + vertexStart);
                    }
                    delete[] buf;
                    break;
                }
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
                    uint16_t* buf = new uint16_t[accessor.count];
                    memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint16_t));
                    for (size_t index = 0; index < accessor.count; index++) {
                        indexes.push_back(buf[index] + vertexStart);
                    }
                    delete[] buf;
                    break;
                }
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
                    uint8_t* buf = new uint8_t[accessor.count];
                    memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint8_t));
                    for (size_t index = 0; index < accessor.count; index++) {
                        indexec.push_back(buf[index] + vertexStart);
                    }
                    delete[] buf;
                    break;
                }
                default:
                    std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
                    return;
                }
            }
            Primitive primitive{};
            primitive.firstIndex = firstIndex;
            primitive.indexCount = indexCount;
            primitive.materialIndex = glTFPrimitive.material;
            node.mesh.primitives.push_back(primitive);
        }
    }

    if (parent) {
        parent->childrens.push_back(node);
    }
    else {
        nodes.push_back(node);
    }
}

void vulkanglTF::Model::loadFromFile(std::string filePath, VkQueue transferQueue, VkCommandPool transferCommandPool)
{
    tinygltf::Model gltfModel;
    tinygltf::TinyGLTF gltfLoader;

    std::string error, warning;
    bool fileLoaded = gltfLoader.LoadASCIIFromFile(&gltfModel, &error, &warning, filePath);

    if (!fileLoaded) {
        throw MakeErrorInfo("glTF: Failed to load model from file!");
    }

    loadImages(gltfModel);
    loadMaterials(gltfModel);
    loadTextures(gltfModel);
    const tinygltf::Scene& scene = gltfModel.scenes[0];
    for (size_t i = 0; i < scene.nodes.size(); i++) {
        const tinygltf::Node node = gltfModel.nodes[scene.nodes[i]];
        loadNode(node, gltfModel, nullptr, vertexes, indexes);
    }

    // Create and upload vertex and index buffer
    // We will be using one single vertex buffer and one single index buffer for the whole glTF scene
    // Primitives (of the glTF model) will then index into these using index offsets

    size_t vertexBufferSize = vertexes.size() * sizeof(vulkanglTF::Vertex);
    size_t indexBufferSize = indexes.size() * sizeof(uint32_t);
    vertexBuffer.count = static_cast<uint32_t>(vertexes.size());
    indexBuffer.count = static_cast<uint32_t>(indexes.size());
    vertexBuffer.vulkanBuffer = new VulkanBuffer(vulkanDevice, vmaAllocator);
    indexBuffer.vulkanBuffer = new VulkanBuffer(vulkanDevice, vmaAllocator);
    vertexBuffer.vulkanBuffer->createBuffer(
        vertexBufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    indexBuffer.vulkanBuffer->createBuffer(
        indexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    // Create staging buffers
    VulkanBuffer vertexStaging(vulkanDevice, vmaAllocator);
    VulkanBuffer indexStaging(vulkanDevice, vmaAllocator);
    vertexStaging.createBuffer(
        vertexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        vertexes.data(),
        vertexBufferSize
    );
    indexStaging.createBuffer(
        indexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        indexes.data(),
        indexBufferSize
    );

    // Copy data from staging buffer to GPU memory
    VkCommandBuffer copyCommandBuffer = vulkanDevice->beginSingleTimeCommands(transferCommandPool);
    VkBufferCopy copyRegion{};
    copyRegion.size = vertexBufferSize;
    vkCmdCopyBuffer(
        copyCommandBuffer,
        vertexStaging.buffer,
        vertexBuffer.vulkanBuffer->buffer,
        1,
        &copyRegion
    );

    copyRegion.size = indexBufferSize;
    vkCmdCopyBuffer(
        copyCommandBuffer,
        indexStaging.buffer,
        indexBuffer.vulkanBuffer->buffer,
        1,
        &copyRegion
    );

    vulkanDevice->endSingleTimeCommands(copyCommandBuffer, transferQueue, transferCommandPool);

    vertexStaging.destroy();
    indexStaging.destroy();
}

VkVertexInputBindingDescription vulkanglTF::Vertex::vertexInputBindingDescription;
std::vector<VkVertexInputAttributeDescription> vulkanglTF::Vertex::vertexInputAttributeDescriptions;
VkPipelineVertexInputStateCreateInfo vulkanglTF::Vertex::pipelineVertexInputStateCreateInfo;

VkVertexInputBindingDescription vulkanglTF::Vertex::inputBindingDescription(uint32_t binding)
{
    return VkVertexInputBindingDescription({ binding, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX });
}

VkVertexInputAttributeDescription vulkanglTF::Vertex::inputAttributeDescription(uint32_t binding, uint32_t location, VertexComponent component)
{
    switch (component) {
    case VertexComponent::Position:
        return VkVertexInputAttributeDescription({ location, binding, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos) });
    case VertexComponent::Normal:
        return VkVertexInputAttributeDescription({ location, binding, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal) });
    case VertexComponent::UV:
        return VkVertexInputAttributeDescription({ location, binding, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv) });
    case VertexComponent::Color:
        return VkVertexInputAttributeDescription({ location, binding, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, color) });
    case VertexComponent::Tangent:
        return VkVertexInputAttributeDescription({ location, binding, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, tangent) });
    case VertexComponent::Joint0:
        return VkVertexInputAttributeDescription({ location, binding, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, joint0) });
    case VertexComponent::Weight0:
        return VkVertexInputAttributeDescription({ location, binding, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, weight0) });
    default:
        return VkVertexInputAttributeDescription({});
    }
}

std::vector<VkVertexInputAttributeDescription> vulkanglTF::Vertex::inputAttributeDescriptions(uint32_t binding, const std::vector<VertexComponent> components)
{
    std::vector<VkVertexInputAttributeDescription> result;
    uint32_t location = 0;
    for (VertexComponent component : components) {
        result.push_back(Vertex::inputAttributeDescription(binding, location, component));
        location++;
    }
    return result;
}

// Get the default pipeline vertex input state create info structure for the requested vertex components
VkPipelineVertexInputStateCreateInfo* vulkanglTF::Vertex::getPipelineVertexInputState(const std::vector<VertexComponent> components)
{
    vertexInputBindingDescription = Vertex::inputBindingDescription(0);
    Vertex::vertexInputAttributeDescriptions = Vertex::inputAttributeDescriptions(0, components);
    pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
    pipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = &Vertex::vertexInputBindingDescription;
    pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(Vertex::vertexInputAttributeDescriptions.size());
    pipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = Vertex::vertexInputAttributeDescriptions.data();
    return &pipelineVertexInputStateCreateInfo;
}

void vulkanglTF::Model::bindBuffers(VkCommandBuffer commandBuffer)
{
    const VkDeviceSize offsets[1] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer.vulkanBuffer->buffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer.vulkanBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
    buffersBound = true;
}

void vulkanglTF::Model::drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, Node node)
{
    if (node.mesh.primitives.size() > 0) {
        // Pass the node's matrix via push constants
        // Traverse the node hierarchy to the top-most parent to get the final matrix of the current node
        glm::mat4 nodeMatrix = node.matrix;
        Node* currentParent = node.parent;
        while (currentParent) {
            nodeMatrix = currentParent->matrix * nodeMatrix;
            currentParent = currentParent->parent;
        }
        // Pass the final matrix to the vertex shader using push constants
        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &nodeMatrix);
        for (Primitive& primitive : node.mesh.primitives) {
            if (primitive.indexCount > 0) {
                // Get the texture index for this primitive
                Texture texture = textures[materials[primitive.materialIndex].baseColorTextureIndex];
                // Bind the descriptor for the current primitive's texture
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &images[texture.imageIndex].descriptorSet, 0, nullptr);
                vkCmdDrawIndexed(commandBuffer, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
            }
        }
    }
    for (auto& child : node.childrens) {
        drawNode(commandBuffer, pipelineLayout, child);
    }
}

void vulkanglTF::Model::draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout)
{
    if (!buffersBound) {
        const VkDeviceSize offsets[1] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer.vulkanBuffer->buffer, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer.vulkanBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
        buffersBound = true;
    }
    // Render all nodes at top-level
    for (auto& node : nodes) {
        drawNode(commandBuffer, pipelineLayout, node);
    }
}
