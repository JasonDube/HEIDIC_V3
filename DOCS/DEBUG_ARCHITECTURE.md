# EDEN ENGINE Debug & Validation Architecture

## Overview

Vulkan validation layers and debug utilities are **essential** for catching errors early. Without them, Vulkan errors often manifest as silent failures, crashes, or undefined behavior.

## Current State

❌ **No validation layers enabled**  
❌ **No debug callbacks**  
❌ **No object naming/labeling**  
❌ **No performance markers**  
❌ **No error reporting infrastructure**

## Required Components

### 1. Validation Layers

**What they catch:**
- Invalid API usage
- Memory leaks
- Synchronization errors
- Resource lifetime issues
- Shader compilation errors
- Descriptor set mismatches

**Implementation:**
```cpp
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"  // All-in-one validation layer (Vulkan 1.3+)
};

// Or individual layers (older Vulkan):
// "VK_LAYER_LUNARG_standard_validation"  // Deprecated, use above
```

### 2. Debug Messenger (VK_EXT_debug_utils)

**Purpose**: Receive validation layer messages and route them to console/logs.

**Features:**
- Filter by severity (ERROR, WARNING, INFO, VERBOSE)
- Filter by message type
- Custom callback functions
- Object naming for better error messages

**Implementation:**
```cpp
VkDebugUtilsMessengerEXT g_debugMessenger = VK_NULL_HANDLE;

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}
```

### 3. VK_LAYER_PATH Configuration

**Purpose**: Tell Vulkan where to find validation layer DLLs.

**Windows Setup:**
```cpp
// In heidic_init_renderer() or before vkCreateInstance()
// Set VK_LAYER_PATH environment variable if not already set
const char* vkLayerPath = getenv("VK_LAYER_PATH");
if (!vkLayerPath) {
    // Default: Vulkan SDK location
    // C:\VulkanSDK\<version>\Bin
    std::string sdkPath = std::getenv("VULKAN_SDK");
    if (!sdkPath.empty()) {
        std::string layerPath = sdkPath + "\\Bin";
        _putenv_s("VK_LAYER_PATH", layerPath.c_str());
    }
}
```

**Better Approach**: Use `vkEnumerateInstanceLayerProperties()` to check availability.

### 4. Object Naming & Labels

**Purpose**: Make validation layer errors more readable.

**Example:**
```cpp
// Name a buffer for better error messages
void setDebugName(VkDevice device, VkObjectType type, uint64_t handle, const char* name) {
    VkDebugUtilsObjectNameInfoEXT nameInfo = {};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = type;
    nameInfo.objectHandle = handle;
    nameInfo.pObjectName = name;
    
    auto func = (PFN_vkSetDebugUtilsObjectNameEXT)
        vkGetInstanceProcAddr(g_instance, "vkSetDebugUtilsObjectNameEXT");
    if (func) {
        func(device, &nameInfo);
    }
}

// Usage:
setDebugName(g_device, VK_OBJECT_TYPE_BUFFER, (uint64_t)g_cubeVertexBuffer, "Cube Vertex Buffer");
setDebugName(g_device, VK_OBJECT_TYPE_IMAGE, (uint64_t)g_textureImage, "Test Texture");
```

### 5. Performance Markers (Optional but Useful)

**Purpose**: Profile GPU work, identify bottlenecks.

**Implementation:**
```cpp
void beginDebugLabel(VkCommandBuffer cmd, const char* label, float r, float g, float b) {
    VkDebugUtilsLabelEXT labelInfo = {};
    labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    labelInfo.pLabelName = label;
    labelInfo.color[0] = r;
    labelInfo.color[1] = g;
    labelInfo.color[2] = b;
    labelInfo.color[3] = 1.0f;
    
    auto func = (PFN_vkCmdBeginDebugUtilsLabelEXT)
        vkGetInstanceProcAddr(g_instance, "vkCmdBeginDebugUtilsLabelEXT");
    if (func) {
        func(cmd, &labelInfo);
    }
}

void endDebugLabel(VkCommandBuffer cmd) {
    auto func = (PFN_vkCmdEndDebugUtilsLabelEXT)
        vkGetInstanceProcAddr(g_instance, "vkCmdEndDebugUtilsLabelEXT");
    if (func) {
        func(cmd);
    }
}
```

## Implementation Plan

### Phase 1: Basic Validation Layers (Priority: HIGH)

**Files to modify:**
- `heidic_v2/vulkan/eden_vulkan_helpers.cpp`

**Changes:**
1. Add validation layer check function
2. Enable validation layers in instance creation
3. Add debug messenger creation
4. Add debug callback function

**Code Structure:**
```cpp
// Check if validation layers are available
bool checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    
    for (const char* layerName : validationLayers) {
        bool layerFound = false;
        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }
        if (!layerFound) {
            return false;
        }
    }
    return true;
}

// Debug callback
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    
    // Filter: Only show errors and warnings in release, everything in debug
    #ifdef NDEBUG
    if (messageSeverity < VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        return VK_FALSE;
    }
    #endif
    
    std::cerr << "[Vulkan Validation] ";
    
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        std::cerr << "ERROR: ";
    } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "WARNING: ";
    } else {
        std::cerr << "INFO: ";
    }
    
    std::cerr << pCallbackData->pMessage << std::endl;
    
    return VK_FALSE;  // Don't abort
}
```

### Phase 2: Debug Utils Extension (Priority: HIGH)

**Changes:**
1. Enable `VK_EXT_debug_utils` extension
2. Create debug messenger
3. Add object naming helper functions

**Heidic API:**
```heidic
// Optional: Allow Heidic code to name objects
extern fn heidic_set_debug_name(object_type: i32, object_handle: u64, name: string): void;
```

### Phase 3: Performance Markers (Priority: MEDIUM)

**Changes:**
1. Add begin/end label functions
2. Integrate into render loop
3. Add ImGui performance panel

**Usage:**
```cpp
// In heidic_begin_frame()
beginDebugLabel(g_commandBuffers[g_currentFrame], "Frame Render", 1.0f, 0.0f, 0.0f);

// In heidic_draw_mesh()
beginDebugLabel(g_commandBuffers[g_currentFrame], "Draw Mesh", 0.0f, 1.0f, 0.0f);
// ... draw code ...
endDebugLabel(g_commandBuffers[g_currentFrame]);

// End frame
endDebugLabel(g_commandBuffers[g_currentFrame]);
```

### Phase 4: Enhanced Error Reporting (Priority: MEDIUM)

**Features:**
- Log file output
- Error counting
- Crash dumps on validation errors (optional)
- ImGui debug panel showing validation stats

**Implementation:**
```cpp
struct ValidationStats {
    uint32_t errorCount = 0;
    uint32_t warningCount = 0;
    std::vector<std::string> recentErrors;
    std::vector<std::string> recentWarnings;
};

static ValidationStats g_validationStats;

// In debug callback:
if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    g_validationStats.errorCount++;
    g_validationStats.recentErrors.push_back(pCallbackData->pMessage);
    if (g_validationStats.recentErrors.size() > 10) {
        g_validationStats.recentErrors.erase(g_validationStats.recentErrors.begin());
    }
}
```

## Configuration Options

### Environment Variables

**VK_LAYER_PATH**: Where to find validation layer DLLs
```bash
# Windows
set VK_LAYER_PATH=C:\VulkanSDK\1.4.328.1\Bin

# Linux
export VK_LAYER_PATH=/usr/share/vulkan/explicit_layer.d
```

**VK_LOADER_DEBUG**: Enable loader debug messages
```bash
set VK_LOADER_DEBUG=all
```

**VK_INSTANCE_LAYERS**: Force enable layers (legacy)
```bash
set VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation
```

### Runtime Configuration

**Heidic API:**
```heidic
// Enable/disable validation layers at runtime
extern fn heidic_enable_validation(enabled: i32): void;

// Set validation message filter
extern fn heidic_set_validation_filter(show_errors: i32, show_warnings: i32, show_info: i32): void;
```

## File Structure

```
heidic_v2/
├── vulkan/
│   ├── eden_vulkan_helpers.h/cpp
│   ├── debug/
│   │   ├── validation_layers.h/cpp      (Layer checking, setup)
│   │   ├── debug_messenger.h/cpp        (Debug callback, messenger)
│   │   ├── debug_utils.h/cpp            (Object naming, labels)
│   │   └── performance_markers.h/cpp    (GPU profiling)
│   └── shaders/
└── docs/
    └── DEBUG_ARCHITECTURE.md            (This file)
```

## Benefits

1. **Early Error Detection**: Catch Vulkan API misuse immediately
2. **Better Error Messages**: Object names make errors readable
3. **Performance Insights**: Markers show where GPU time is spent
4. **Memory Leak Detection**: Validation layers track resource lifetime
5. **Synchronization Validation**: Catch race conditions and sync errors

## Next Steps

1. **Implement Phase 1** (Validation layers + debug messenger)
2. **Test with existing code** (should catch any hidden issues)
3. **Add object naming** for all major resources
4. **Add performance markers** to render loop
5. **Create ImGui debug panel** for validation stats

## Notes

- **Performance Impact**: Validation layers add ~10-30% overhead. Disable in release builds.
- **Availability**: Validation layers come with Vulkan SDK. Ensure SDK is installed.
- **Platform Support**: Works on Windows, Linux, macOS (via MoltenVK).

---

**Status**: Design Phase  
**Last Updated**: December 2024  
**Next Milestone**: Implement Phase 1 (Basic Validation Layers)

