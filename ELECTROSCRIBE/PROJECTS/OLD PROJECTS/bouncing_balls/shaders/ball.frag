#version 450

// Fragment shader for bouncing balls - hot-reloadable!
// Try changing colors, adding effects, etc.

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    // Simple solid color (can hot-reload to change this!)
    outColor = vec4(fragColor, 1.0);
    
    // Try uncommenting these for different effects:
    // outColor = vec4(1.0, 0.0, 0.0, 1.0);  // Red
    // outColor = vec4(0.0, 1.0, 0.0, 1.0);  // Green
    // outColor = vec4(0.0, 0.0, 1.0, 1.0);  // Blue
}

