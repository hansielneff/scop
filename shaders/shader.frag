#version 450

layout(location = 0) in flat uint triangleIndex;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in float colorToTextureRatio;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D texSampler;

void main() {
    float colorValue = max(0.01, (triangleIndex % 4) / 4.0);
    vec4 triangleColor = vec4(vec3(colorValue), 1.0f);
    vec4 textureColor = texture(texSampler, fragTexCoord);
    outColor = mix(triangleColor, textureColor, colorToTextureRatio);
}
