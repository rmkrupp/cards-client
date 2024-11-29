#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    ivec2 indices;
} ubo[1];

layout(push_constant, std430) uniform pc {
    mat4 view;
    mat4 projection;
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out flat ivec2 texture_indices;

void main() {
    vec4 pos = vec4(inPosition.xyz, 1.0) * ubo[gl_InstanceIndex].model * view * projection;
    gl_Position = vec4(pos.xy, pos.z, pos.w);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    texture_indices = ubo[gl_InstanceIndex].indices;
}
