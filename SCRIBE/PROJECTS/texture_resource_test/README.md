# TextureResource Test Project

This project tests the **TextureResource** class - a unified texture loader that automatically detects and handles both DDS (compressed) and PNG (uncompressed) textures.

## What This Tests

✅ **Auto-detection** - TextureResource automatically detects DDS vs PNG format  
✅ **Unified API** - Same interface for both compressed and uncompressed textures  
✅ **Vulkan integration** - Creates image, view, and sampler ready for shaders  
✅ **RAII cleanup** - Automatic resource cleanup

## Setup

1. **Add a test texture:**
   - Place either `test.dds` OR `test.png` in the `textures/` folder
   - TextureResource will auto-detect the format!
   - Try both formats to see it works with either

2. **Build and Run:**
   - Open this project in SCRIBE editor
   - Click the white "Run" button (▶)
   - You should see your texture displayed on a fullscreen quad!

## How It Works

The `heidic_init_renderer_texture_quad()` function:
1. Creates a `TextureResource` instance with your texture path
2. TextureResource automatically detects if it's DDS or PNG
3. Loads the texture using the appropriate loader (DDS or PNG)
4. Creates Vulkan resources (image, view, sampler)
5. Sets up rendering pipeline
6. Displays the texture on a fullscreen quad

## Comparison

**Before (manual):**
- Need to call `load_dds()` or `load_png()` manually
- Need to create Vulkan resources manually
- Different code paths for DDS vs PNG

**With TextureResource:**
```cpp
TextureResource tex("textures/test.dds");  // Works!
TextureResource tex2("textures/test.png"); // Also works!
// Same API, auto-detects format!
```

This is the foundation for the future HEIDIC zero-boilerplate syntax:
```heidic
resource Texture = "textures/test.dds";  // Coming in Phase 2!
```

