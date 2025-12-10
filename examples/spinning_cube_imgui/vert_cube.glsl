#version 450

// Vertex input (from vertex buffer)
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

// Uniform buffer (model, view, projection matrices)
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

// Output to fragment shader
layout(location = 0) out vec3 fragColor;

void main() {
    // Transform vertex position
    vec4 pos = vec4(inPosition, 1.0);
    pos = ubo.model * pos;
    pos = ubo.view * pos;
    pos = ubo.proj * pos;
    gl_Position = pos;
    
    // Pass color to fragment shader
    fragColor = inColor;
}

