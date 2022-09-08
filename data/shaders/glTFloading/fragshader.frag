#version 450

layout(location = 0) in vec2 inUV;

layout(set = 1, binding = 0) uniform sampler2D colorTexture;

layout(location = 0) out vec4 outFragColor;

void main()
{
    outFragColor = texture(colorTexture, inUV);
}