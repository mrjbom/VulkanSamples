#version 450

layout(location = 0) out vec4 outColor;

layout (constant_id = 0) const float vertexColorR = 0.0f;
layout (constant_id = 1) const float vertexColorG = 0.0f;
layout (constant_id = 2) const float vertexColorB = 0.0f;

void main() {
    outColor = vec4(vertexColorR, vertexColorG, vertexColorB, 1.0f);
}