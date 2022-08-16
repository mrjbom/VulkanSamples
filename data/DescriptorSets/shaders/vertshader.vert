#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(binding = 0) uniform matrixes {
    mat4 projection;
    mat4 view;
    mat4 model;
} matrix;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = matrix.projection * matrix.view * matrix.model * vec4(inPosition, 1.0);
    fragColor = inColor;
}