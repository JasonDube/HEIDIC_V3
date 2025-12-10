// EDEN ENGINE Standard Library - GLFW Bindings
// This file is automatically included in generated C++ code

#ifndef EDEN_GLFW_H
#define EDEN_GLFW_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdint.h>

// GLFW types - use GLFW's native types
// GLFWwindow is already GLFWwindow* in GLFW, so we use it directly
// For EDEN, GLFWwindow means GLFWwindow* (the pointer type)

// GLFW constants
#define GLFW_TRUE 1
#define GLFW_FALSE 0
#define GLFW_RELEASE 0
#define GLFW_PRESS 1
#define GLFW_REPEAT 2

// GLFW window hints
#define GLFW_CLIENT_API 0x00022001
#define GLFW_NO_API 0

// GLFW key codes
#define GLFW_KEY_SPACE 32
#define GLFW_KEY_ESCAPE 256
#define GLFW_KEY_ENTER 257
#define GLFW_KEY_W 87
#define GLFW_KEY_A 65
#define GLFW_KEY_S 83
#define GLFW_KEY_D 68

// GLFW functions are already declared in glfw3.h
// We don't need to redeclare them - just use GLFW's native declarations
// The functions are available via the GLFW header

// Common GLFW functions that can be called from HEIDIC:
// - glfwInit(): i32
// - glfwTerminate(): void
// - glfwCreateWindow(width: i32, height: i32, title: *const u8, monitor: *void, share: *void): GLFWwindow
// - glfwDestroyWindow(window: GLFWwindow): void
// - glfwWindowShouldClose(window: GLFWwindow): i32
// - glfwSetWindowShouldClose(window: GLFWwindow, value: i32): void
// - glfwPollEvents(): void
// - glfwGetKey(window: GLFWwindow, key: i32): i32
// - glfwGetTime(): f64  // Returns time in seconds since GLFW was initialized

#endif // EDEN_GLFW_H

