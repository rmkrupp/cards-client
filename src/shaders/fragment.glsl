#version 450
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(binding = 1) uniform sampler2D texSamplerOutline;
layout(binding = 2) uniform sampler2D texSamplerSolid;

layout(location = 0) out vec4 out_color;

void main() {
    float t_solid = texture(texSamplerSolid, fragTexCoord).x;
    float t_outline = texture(texSamplerOutline, fragTexCoord).x;
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
