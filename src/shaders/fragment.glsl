#version 450
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in flat ivec2 texture_indices;

layout(binding = 1) uniform sampler2DArray texSampler;
//layout(binding = 2) uniform sampler2D texSamplerSolid;

layout(location = 0) out vec4 out_color;

void main() {
    float t_solid = texture(texSampler, vec3(fragTexCoord, texture_indices.x)).x;
    float t_outline = texture(texSampler, vec3(fragTexCoord, texture_indices.y)).x;
    if (t_solid > 0.5) {
        discard;
    } else {
        if (t_outline > 0.5) {
            out_color = vec4(1.0, 1.0, 1.0, 1.0);
        } else {
            out_color = vec4(0.0, 0.0, 0.0, 1.0);
        }
    }
}
