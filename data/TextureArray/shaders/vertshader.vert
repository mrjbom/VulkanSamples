#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTextureCoord;

layout(push_constant) uniform PushConstantData {
    mat4 MVPmatrix;
    float lodBias;
    float currentLayer;
} pushConstantData;

layout(location = 0) out vec2 outTextureCoord;
layout(location = 1) out float outLodBias;
layout(location = 2) out float outCurrentLayer;

void main()
{
    gl_Position = pushConstantData.MVPmatrix * vec4(inPosition, 1.0);
    outTextureCoord = inTextureCoord;
    outLodBias = pushConstantData.lodBias.x;
    outCurrentLayer = pushConstantData.currentLayer.x;
}