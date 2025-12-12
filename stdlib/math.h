#pragma once

// EDEN ENGINE Math Library - GLM Compatibility Wrapper
// This header maintains the EDEN math API while using GLM internally
// Custom types (Vec2, Vec3, Vec4, Mat4) are mapped to GLM types

// Include GLM (adjust path if needed)
// If GLM is in third_party/glm/glm/, add -Ithird_party to compiler flags
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>  // For value_ptr
#include <cstring>
#include <cmath>  // For sinf, cosf, sqrtf

// Custom types that wrap GLM internally
// Store data directly for compatibility, convert to GLM when needed
struct Vec2 {
    float x, y;
    Vec2() : x(0.0f), y(0.0f) {}
    Vec2(float x, float y) : x(x), y(y) {}
    Vec2(const glm::vec2& v) : x(v.x), y(v.y) {}
    operator glm::vec2() const { return glm::vec2(x, y); }
};

struct Vec3 {
    float x, y, z;
    Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    Vec3(const glm::vec3& v) : x(v.x), y(v.y), z(v.z) {}
    operator glm::vec3() const { return glm::vec3(x, y, z); }
};

struct Vec4 {
    float x, y, z, w;
    Vec4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
    Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    Vec4(const glm::vec4& v) : x(v.x), y(v.y), z(v.z), w(v.w) {}
    operator glm::vec4() const { return glm::vec4(x, y, z, w); }
};

struct Mat4 {
    glm::mat4 data;
    float m[16]; // Compatibility accessor (column-major)
    
    Mat4() : data(1.0f) { updateM(); }
    Mat4(const glm::mat4& mat) : data(mat) { updateM(); }
    operator glm::mat4() const { return data; }
    
    static Mat4 identity() {
        Mat4 res;
        res.data = glm::mat4(1.0f);
        res.updateM();
        return res;
    }
    
    void updateM() {
        // Copy GLM matrix to m[] array (column-major)
        // GLM matrices are column-major, so we access by column then row
        const glm::mat4& mat = data;
        for (int col = 0; col < 4; col++) {
            for (int row = 0; row < 4; row++) {
                m[col * 4 + row] = mat[col][row];
            }
        }
    }
    
    Mat4& operator=(const glm::mat4& mat) {
        data = mat;
        updateM();
        return *this;
    }
};

// Vector operations (wrap GLM)
inline Vec3 vec3_add(Vec3 a, Vec3 b) { return Vec3(glm::vec3(a) + glm::vec3(b)); }
inline Vec3 vec3_sub(Vec3 a, Vec3 b) { return Vec3(glm::vec3(a) - glm::vec3(b)); }
inline Vec3 vec3_mul(Vec3 a, float s) { return Vec3(glm::vec3(a) * s); }
inline float vec3_dot(Vec3 a, Vec3 b) { return glm::dot(glm::vec3(a), glm::vec3(b)); }
inline Vec3 vec3_cross(Vec3 a, Vec3 b) { return Vec3(glm::cross(glm::vec3(a), glm::vec3(b))); }
inline Vec3 vec3_normalize(Vec3 a) { 
    glm::vec3 v = glm::vec3(a);
    float len = glm::length(v);
    if (len > 0.0f) return Vec3(v / len);
    return Vec3(0.0f, 0.0f, 0.0f);
}

inline Vec3 vec3_lerp(Vec3 a, Vec3 b, float t) {
    // Linear interpolation: a * (1 - t) + b * t
    return Vec3(glm::mix(glm::vec3(a), glm::vec3(b), t));
}

// Matrix operations (wrap GLM)
inline Mat4 mat4_mul(Mat4 a, Mat4 b) { 
    Mat4 res(glm::mat4(a) * glm::mat4(b));
    return res;
}

inline Mat4 mat4_perspective(float fov_rad, float aspect, float near_plane, float far_plane) {
    // EDEN uses Vulkan coordinate system: Clip Z is [0, 1]
    return Mat4(glm::perspectiveRH_ZO(fov_rad, aspect, near_plane, far_plane));
}

inline Mat4 mat4_lookat(Vec3 eye, Vec3 center, Vec3 up) {
    return Mat4(glm::lookAt(glm::vec3(eye), glm::vec3(center), glm::vec3(up)));
}

inline Mat4 mat4_rotate_z(float angle_rad) {
    return Mat4(glm::rotate(glm::mat4(1.0f), angle_rad, glm::vec3(0.0f, 0.0f, 1.0f)));
}

inline Mat4 mat4_rotate(Vec3 axis, float angle_rad) {
    glm::vec3 axis_norm = glm::normalize(glm::vec3(axis));
    return Mat4(glm::rotate(glm::mat4(1.0f), angle_rad, axis_norm));
}

inline Mat4 mat4_translate(Vec3 translation) {
    return Mat4(glm::translate(glm::mat4(1.0f), glm::vec3(translation)));
}

// C linkage wrappers for EDEN FFI
extern "C" {
    Vec3 eden_vec3(float x, float y, float z);
    Vec3 eden_vec3_add(Vec3 a, Vec3 b);
    float eden_vec3_get_x(Vec3 v);
    float eden_vec3_get_y(Vec3 v);
    float eden_vec3_get_z(Vec3 v);
    Mat4 eden_mat4_perspective(float fov, float aspect, float near, float far);
    Mat4 eden_mat4_lookat(Vec3 eye, Vec3 center, Vec3 up);
    Mat4 eden_mat4_rotate_z(float angle);
    Mat4 eden_mat4_mul(Mat4 a, Mat4 b);
    Mat4 eden_mat4_translate(Vec3 translation);
    
    // Math helper functions for HEIDIC
    inline float heidic_sin(float radians) {
        return sinf(radians);
    }
    
    inline float heidic_cos(float radians) {
        return cosf(radians);
    }
    
    inline float heidic_sqrt(float value) {
        return sqrtf(value);
    }
    
    inline float heidic_convert_degrees_to_radians(float degrees) {
        return degrees * 3.14159265358979323846f / 180.0f;
    }
}

