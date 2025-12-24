// ============================================================================
// FACIAL ANIMATION TYPES - DMap-based Facial Animation System
// ============================================================================
// Implements Sims 4-style UV1 + DMap vertex displacement for facial animation.
// DMaps are RGB textures where R/G/B = X/Y/Z vertex displacement deltas.
// ============================================================================

#ifndef FACIAL_TYPES_H
#define FACIAL_TYPES_H

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <array>

namespace facial {

// ============================================================================
// Constants
// ============================================================================

constexpr int MAX_SLIDERS = 32;           // Maximum number of active facial sliders
constexpr int MAX_DMAP_TEXTURES = 16;     // Maximum DMap textures in atlas
constexpr float DEFAULT_DISP_STRENGTH = 0.05f;  // Default displacement strength (5cm max)

// ============================================================================
// DMap Texture (Displacement Map)
// ============================================================================
// RGB texture where each channel encodes displacement delta:
//   R = X displacement (-1 to 1 mapped to 0-255)
//   G = Y displacement
//   B = Z displacement
// Sampled via UV1 coordinates, scaled by slider weight

struct DMapTexture {
    std::string name;                 // e.g., "jawOpen", "smile", "browRaise"
    std::vector<uint8_t> pixels;      // RGB or RGBA pixel data
    uint32_t width = 0;
    uint32_t height = 0;
    int sliderIndex = -1;             // Which slider controls this DMap (0-31)
    float maxDisplacement = DEFAULT_DISP_STRENGTH;  // Maximum displacement in world units
    bool valid = false;
};

// ============================================================================
// Facial Slider
// ============================================================================
// Runtime control for facial expression. Each slider blends one or more DMaps.

struct FacialSlider {
    std::string name;                 // e.g., "jawOpen", "smile_L", "browRaise_R"
    float weight = 0.0f;              // Current weight (0.0 = neutral, 1.0 = full)
    float minWeight = 0.0f;           // Minimum allowed weight
    float maxWeight = 1.0f;           // Maximum allowed weight
    int dmapIndex = -1;               // Index into DMap texture array
    bool enabled = true;              // Can be disabled to skip processing
};

// ============================================================================
// Expression Preset
// ============================================================================
// Predefined combination of slider weights for common expressions

struct ExpressionPreset {
    std::string name;                 // e.g., "smile", "frown", "surprise"
    std::array<float, MAX_SLIDERS> weights = {};  // Weight for each slider
};

// ============================================================================
// Facial Slider UBO (GPU-side)
// ============================================================================
// Uniform buffer for shader - contains slider weights and displacement settings

struct FacialUBO {
    // Slider weights (32 floats = 128 bytes, padded to vec4 for alignment)
    glm::vec4 weights[MAX_SLIDERS / 4];  // weights[0].x = slider 0, weights[0].y = slider 1, etc.
    
    // Global settings
    glm::vec4 settings;  // x = globalStrength, y = mirrorX threshold (0.5), z/w = reserved
    
    FacialUBO() {
        for (int i = 0; i < MAX_SLIDERS / 4; ++i) {
            weights[i] = glm::vec4(0.0f);
        }
        settings = glm::vec4(1.0f, 0.5f, 0.0f, 0.0f);
    }
    
    // Helper to set/get individual slider weights
    void setWeight(int index, float value) {
        if (index >= 0 && index < MAX_SLIDERS) {
            int vecIndex = index / 4;
            int component = index % 4;
            weights[vecIndex][component] = value;
        }
    }
    
    float getWeight(int index) const {
        if (index >= 0 && index < MAX_SLIDERS) {
            int vecIndex = index / 4;
            int component = index % 4;
            return weights[vecIndex][component];
        }
        return 0.0f;
    }
};

// Verify UBO size (32 vec4s for weights + 1 vec4 settings = 33 * 16 = 528 bytes... wait that's wrong)
// Actually: 8 vec4s for 32 weights + 1 vec4 settings = 9 * 16 = 144 bytes
static_assert(sizeof(FacialUBO) == 144, "FacialUBO size mismatch - check alignment");

// ============================================================================
// Facial Animation Keyframe
// ============================================================================
// Single keyframe in a facial animation clip

struct FacialKeyframe {
    float time = 0.0f;                    // Time in seconds
    std::array<float, MAX_SLIDERS> weights = {};  // Slider weights at this keyframe
};

// ============================================================================
// Facial Animation Clip
// ============================================================================
// Sequence of keyframes for animating facial expressions

struct FacialAnimationClip {
    std::string name;
    std::vector<FacialKeyframe> keyframes;
    float duration = 0.0f;            // Total duration in seconds
    bool loop = false;                // Whether to loop the animation
    
    // Get interpolated weights at a given time
    void getWeightsAtTime(float time, std::array<float, MAX_SLIDERS>& outWeights) const;
};

// ============================================================================
// Common Expression Presets (helpers)
// ============================================================================

namespace expressions {
    // Standard slider indices (can be remapped per-model)
    enum SliderIndex {
        JAW_OPEN = 0,
        SMILE_L = 1,
        SMILE_R = 2,
        FROWN_L = 3,
        FROWN_R = 4,
        BROW_RAISE_L = 5,
        BROW_RAISE_R = 6,
        BROW_FURROW = 7,
        EYE_WIDE_L = 8,
        EYE_WIDE_R = 9,
        EYE_SQUINT_L = 10,
        EYE_SQUINT_R = 11,
        NOSE_WRINKLE = 12,
        LIP_PUCKER = 13,
        CHEEK_PUFF_L = 14,
        CHEEK_PUFF_R = 15,
        // Add more as needed up to MAX_SLIDERS - 1
    };
}

} // namespace facial

#endif // FACIAL_TYPES_H

