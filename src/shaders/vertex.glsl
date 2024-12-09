#version 450
    
struct object {
    mat4 model;
    uint solid_index, outline_index, glow_index;
    uint flags;
};

layout(binding = 0, std140) uniform UniformBufferObject {
    object objects[3];
} ubo;

layout(push_constant, std430) uniform pc {
    mat4 view;
    mat4 projection;
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out flat ivec3 texture_indices;

void main() {
    if (ubo.objects[gl_InstanceIndex].flags == 0) {
        gl_Position = vec4(0.0, 0.0, -10.0, 1.0);
    } else {
        vec4 pos = vec4(inPosition, 1.0) * ubo.objects[gl_InstanceIndex].model * view * projection;
        gl_Position = vec4(pos.xy, pos.z, pos.w);
    }
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    texture_indices = ivec3(ubo.objects[gl_InstanceIndex].solid_index, ubo.objects[gl_InstanceIndex].outline_index, ubo.objects[gl_InstanceIndex].glow_index);
    vec4 normal = vec4(inNormal, 1.0) * ubo.objects[gl_InstanceIndex].model * view * projection;
    fragNormal = normalize(normal.xyz);
}
