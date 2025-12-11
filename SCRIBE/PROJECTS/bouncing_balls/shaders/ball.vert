#version 450

// Vertex shader for bouncing balls
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

// Uniform buffer for view/projection (shared)
layout(binding = 0) uniform UniformBufferObject {
    mat4 model; // kept for compatibility, but model comes from push constants
    mat4 view;
    mat4 proj;
} ubo;

// Per-draw model matrix via push constants (avoids all draws sharing last model)
layout(push_constant) uniform Push {
    mat4 model;
} pc;

void main() {
    gl_Position = ubo.proj * ubo.view * pc.model * vec4(inPosition, 1.0);
    fragColor = inColor;
}

