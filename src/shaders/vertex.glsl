#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 projection;
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

    void main() {
    gl_Position = vec4(inPosition.xyz, 1.0) * model * view * projection;
    fragColor = inColor;
}
