#version 450

layout(location = 0) in flat uint vertexIndex;

layout(push_constant) uniform ColorsPushConstant
{
  layout(offset = 64) vec4 vertexColors[3];
} colorsPushConstant;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(colorsPushConstant.vertexColors[vertexIndex]);
}