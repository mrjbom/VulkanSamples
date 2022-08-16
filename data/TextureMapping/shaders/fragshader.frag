#version 450

layout(binding = 0) uniform sampler2D samplerColor;

layout(location = 0) in vec2 inTextureCoord;
layout(location = 1) in float inLodBias;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(samplerColor, inTextureCoord, inLodBias);
}