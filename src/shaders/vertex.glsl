#version 450

struct object {
    mat4 model;
    uint solid_index, outline_index, glow_index;
    uint flags;
};

layout(binding = 0, std140) buffer restrict readonly UniformBufferObject {
    object objects[];
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
layout(location = 1) out vec3 fragWorldPosition;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec2 fragTexCoord;
layout(location = 4) out flat ivec3 texture_indices;
layout(location = 5) out flat uint fragFlags;

void main() {
    uint flags = ubo.objects[gl_InstanceIndex].flags;

    if ((flags & 1) == 0) {
        // disabled
        gl_Position = vec4(0.0, 0.0, -10.0, 1.0);
    } else if ((flags & 4) == 4) {
        // rain

        vec3 position = ubo.objects[gl_InstanceIndex].model[0].xyz;
        vec4 rotation = ubo.objects[gl_InstanceIndex].model[1];
        float scale = ubo.objects[gl_InstanceIndex].model[2].x;
        float velocity = ubo.objects[gl_InstanceIndex].model[2].y;
        mat4 m_scale_trans = mat4(
            scale, 0.0, 0.0, position.x,
            0.0, scale, 0.0, position.y,
            0.0, 0.0, scale, position.z,
            0.0, 0.0, 0.0, 1.0
        );
        mat4 m_rot = mat4(
            1 - 2 * rotation.y * rotation.y - 2 * rotation.z * rotation.z,
            2 * rotation.x * rotation.y - 2 * rotation.w * rotation.z,
            2 * rotation.x * rotation.z + 2 * rotation.w * rotation.y,
            0,

            2 * rotation.x * rotation.y + 2 * rotation.w * rotation.z,
            1 - 2 * rotation.x * rotation.x - 2 * rotation.z * rotation.z,
            2 * rotation.y * rotation.z - 2 * rotation.w * rotation.x,
            0,

            2 * rotation.x * rotation.z - 2 * rotation.w * rotation.y,
            2 * rotation.y * rotation.z + 2 * rotation.w * rotation.x,
            1 - 2 * rotation.x * rotation.x - 2 * rotation.y * rotation.y,
            0,

            0,
            0,
            0,
            1
        );

        vec4 worldPosition = vec4(inPosition, 1.0) * m_scale_trans * m_rot; 
        gl_Position = worldPosition * view * projection;
        fragWorldPosition = worldPosition.xyz;
        fragColor = inColor;
        fragTexCoord = inTexCoord;
        texture_indices = ivec3(ubo.objects[gl_InstanceIndex].solid_index, ubo.objects[gl_InstanceIndex].outline_index, ubo.objects[gl_InstanceIndex].glow_index);
        vec4 normal = vec4(inNormal, 1.0) * m_scale_trans * view * projection;
        fragNormal = normalize(normal.xyz);
        fragFlags = flags;
    } else {
        vec4 worldPosition = vec4(inPosition, 1.0) * ubo.objects[gl_InstanceIndex].model;
        gl_Position = worldPosition * view * projection;
        fragWorldPosition = worldPosition.xyz;
        fragColor = inColor;
        fragTexCoord = inTexCoord;
        texture_indices = ivec3(ubo.objects[gl_InstanceIndex].solid_index, ubo.objects[gl_InstanceIndex].outline_index, ubo.objects[gl_InstanceIndex].glow_index);
        vec4 normal = vec4(inNormal, 1.0) * ubo.objects[gl_InstanceIndex].model * view * projection;
        fragNormal = normalize(normal.xyz);
        fragFlags = flags;
    }
}
