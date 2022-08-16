#version 450

layout(location = 0) in vec3 inPosition;

layout(push_constant) uniform MatrixPushConstant
{
	mat4 MVPmatrix;
} matrixPushConstant;

layout(location = 0) out flat uint vertexIndex;

void main() {
    gl_Position = matrixPushConstant.MVPmatrix * vec4(inPosition, 1.0);
    vertexIndex = gl_VertexIndex;
}