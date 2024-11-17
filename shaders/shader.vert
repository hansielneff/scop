#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out uint triangleIndex;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out float colorToTextureRatio;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    float colorToTextureRatio;
} ubo;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    triangleIndex = gl_VertexIndex / 3;
    fragTexCoord = inTexCoord;
    colorToTextureRatio = ubo.colorToTextureRatio;
}
