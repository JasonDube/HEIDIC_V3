#version 450

// Vertex input (POSITION_COLOR format)
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

// Uniform buffer (view, projection only - model is in push constants)
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;   // Unused by old sl_engine, but must be here for padding
    mat4 view;
    mat4 proj;
} ubo;

// Push constants (per-object model matrix + color)
// Old sl_engine.cpp uses this!
layout(push_constant) uniform PushConstants {
    mat4 model;   // 64 bytes
    vec4 color;   // 16 bytes = 80 bytes total
} push;

// Output to fragment shader
layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = ubo.proj * ubo.view * push.model * vec4(inPosition, 1.0);
    
    // Multiply vertex color by object color
    fragColor = inColor * push.color.rgb;
}

