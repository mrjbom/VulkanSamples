#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;

layout(set = 0, binding = 0) uniform BufferMatrixes
{
    mat4 projection;
    mat4 view;
    mat4 model;
} bufferMatrixes;

layout(location = 0) out vec2 outUV;

void main()
{
    gl_Position = bufferMatrixes.projection * bufferMatrixes.view * bufferMatrixes.model * vec4(inPos, 1.0);
    outUV = inUV;
}