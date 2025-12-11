#version 450

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec2 fragUV;

void main() {
    // Fullscreen quad - positions are already in NDC (-1 to 1)
    gl_Position = vec4(inPos, 0.0, 1.0);
    fragUV = inUV;
}

