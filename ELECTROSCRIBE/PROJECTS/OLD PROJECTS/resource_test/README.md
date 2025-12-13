# Resource Syntax Test

This project tests the new HEIDIC `resource` keyword syntax for zero-boilerplate resource loading.

## What This Tests

✅ **Resource Declaration** - Using `resource` keyword to declare resources  
✅ **Zero Boilerplate** - No manual C++ code needed  
✅ **Automatic Loading** - Resources load automatically when declared  
✅ **Model Loading** - Tests loading OBJ meshes with textures

## Setup

1. **Add your model and texture files:**
   - Place `eve_1.obj` in `models/` folder
   - Place `eve_tex.png` in `textures/` folder

2. **Build and Run:**
   - Open this project in SCRIBE editor
   - Click the white "Run" button (▶)
   - The resources will be loaded automatically!

## HEIDIC Resource Syntax

```heidic
// Declare a mesh resource
resource MyMesh: Mesh = "models/eve_1.obj";

// Declare a texture resource
resource MyTexture: Texture = "textures/eve_tex.png";
```

**This generates:**
```cpp
Resource<MeshResource> g_resource_mymesh("models/eve_1.obj");
Resource<TextureResource> g_resource_mytexture("textures/eve_tex.png");
```

## Features

- **Zero Boilerplate** - One line declares and loads the resource
- **RAII Cleanup** - Automatic resource management
- **Hot-Reload Ready** - Resources support CONTINUUM hot-reload
- **Type Safety** - Resource types are checked at compile time

## Next Steps

In the future, we can add:
- Direct resource access in HEIDIC (e.g., `MyMesh.getVertexBuffer()`)
- Resource hot-reload integration with CONTINUUM
- More resource types (glTF, audio, etc.)

