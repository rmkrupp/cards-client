#version 450
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in flat ivec2 texture_indices;

layout(binding = 1) uniform sampler2DArray texSampler;
//layout(binding = 2) uniform sampler2D texSamplerSolid;

layout(location = 0) out vec4 out_color;

void main() {
    float t_solid = texture(texSampler, vec3(fragTexCoord, texture_indices.x)).x;
    float t_outline = texture(texSampler, vec3(fragTexCoord, texture_indices.y)).x;

    float dcenter = distance(fragTexCoord, vec2(0.5, 0.5));
    float dcorner00 = distance(fragTexCoord, vec2(0.0, 0.0));
    float dcorner01 = distance(fragTexCoord, vec2(0.0, 1.0));
    float dcorner10 = distance(fragTexCoord, vec2(1.0, 0.0));
    float dcorner11 = distance(fragTexCoord, vec2(1.0, 1.0));

    float threshold = 0.025;

    float mind = min(dcenter, min(dcorner00, min(dcorner01, min(dcorner10, dcorner11))));

    float outline_cutoff = 0.0625;
    float outline_cutoff2 = 0.0625;
    /*
    if (dFdx(t_outline) > 0.0 || dFdy(t_outline) > 0.01) {
        outline_cutoff2 = 0.01;
    }
    */
    //float outline_cutoff2 = outline_cutoff * (dFdx(t_outline)) * (dFdy(t_outline));

    float x = fragNormal.z;


    if (t_solid > 0.5 * 16 / 128) {
        discard;
    } else if (t_outline >= outline_cutoff) {
        out_color = vec4(1.0, 1.0, 1.0, 1.0);
    } else {
        out_color = vec4(0.0, 0.0, 0.0, 1.0);
    }
    /*
    if (mind > threshold && t_solid > (0.5 * 16 / 128)) {
        discard;
    } else {
        if (mind <= threshold) {
            out_color = vec4(0.0, 1.0, 0.0, 1.0);
        } else if (t_outline >= outline_cutoff) {
            out_color = vec4(1.0, 1.0, 1.0, 1.0);
        } else {
            out_color = vec4(0.0, 0.0, 0.0, 1.0);
        }
    }
    */

}
