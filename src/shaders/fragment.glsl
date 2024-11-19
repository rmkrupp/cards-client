#version 450
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) out vec4 out_color;

void main() {
    float t = texture(texSampler, fragTexCoord).x;
    if (t > 0.5) {
        discard;
    } else {
        out_color = vec4(0.0, 0.0, 0.0, 1.0);
    }
}
