#version 450

layout(location = 0) in vec3 inPosition;

layout(push_constant) uniform MatrixPushConstant
{
	mat4 MVPmatrix;
} matrixPushConstant;

void main() {
    gl_Position = matrixPushConstant.MVPmatrix * vec4(inPosition, 1.0);
}