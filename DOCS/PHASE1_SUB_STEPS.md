# Phase 1: Engine Runtime - Sub-Steps Overview

## Goal
Build the C++ infrastructure that HEIDIC will generate code to use. This is the foundation.

---

## Phase 1.1: DDS Loader (`stdlib/dds_loader.h`)

**Purpose:** Parse DDS files and extract compressed texture data ready for GPU upload.

**Sub-tasks:**
1. **Define DDS header structures**
   - Magic number ("DDS ")
   - Header struct (width, height, mipmaps, format info)
   - Pixel format struct (FourCC codes)
   - Surface description flags

2. **Write DDS file reader**
   - Read file from disk (or memory-map)
   - Validate magic number
   - Parse header structures
   - Extract format (BC7, BC5, R8, etc.)

3. **Map DDS formats to Vulkan formats**
   - BC7 → `VK_FORMAT_BC7_UNORM_BLOCK`
   - BC5 → `VK_FORMAT_BC5_UNORM_BLOCK`
   - BC1/DXT1 → `VK_FORMAT_BC1_RGB_UNORM_BLOCK`
   - BC3/DXT5 → `VK_FORMAT_BC3_UNORM_BLOCK`
   - R8 → `VK_FORMAT_R8_UNORM`

4. **Extract compressed data blocks**
   - Read mipmap levels
   - Extract compressed texture data
   - Calculate data offsets (DDS layout)

5. **Return structured data**
   ```cpp
   struct DDSData {
       VkFormat format;
       uint32_t width, height;
       uint32_t mipmapCount;
       std::vector<uint8_t> compressedData;  // Ready for GPU
   };
   ```

**Dependencies:** None (pure file I/O)

**Testing:** Load a DDS file, verify format detection, verify data extraction.

---

## Phase 1.2: PNG Loader (`stdlib/png_loader.h`)

**Purpose:** Load PNG files for source assets (slower path, but needed for workflow).

**Sub-tasks:**
1. **Choose PNG library**
   - Option A: Use `stb_image.h` (header-only, single file)
   - Option B: Write minimal PNG parser (more work, zero dependencies)
   - **Recommendation:** Start with stb_image (fast, proven)

2. **Write PNG loader function**
   - Load PNG file
   - Extract width, height, channels
   - Get RGBA8 pixel data
   - Handle different PNG formats (RGB, RGBA, grayscale)

3. **Return structured data**
   ```cpp
   struct PNGData {
       uint32_t width, height;
       uint32_t channels;  // 3 or 4
       std::vector<uint8_t> pixels;  // RGBA8 format
   };
   ```

**Dependencies:** `stb_image.h` (or custom parser)

**Testing:** Load a PNG file, verify pixel data extraction, verify format conversion.

**Note:** This is the "slow path" - warn if used at runtime. Primary purpose is for build-time conversion to DDS.

---

## Phase 1.3: Texture Resource Class (`stdlib/texture_resource.h`)

**Purpose:** High-level texture resource that handles loading, Vulkan creation, and lifecycle.

**Sub-tasks:**
1. **Create TextureResource class structure**
   - Member variables (VkImage, VkImageView, VkSampler, VkDeviceMemory)
   - Dimensions (width, height)
   - Format (VkFormat)
   - Path (for hot-reload)

2. **Implement constructor**
   - Take file path
   - Detect file format (`.dds`, `.ktx2`, `.png`)
   - Route to appropriate loader (DDS fast path, PNG slow path)

3. **Implement DDS loading path**
   - Call `load_dds()`
   - Create Vulkan image with correct format (BC7, BC5, etc.)
   - Allocate GPU memory
   - Upload compressed data directly (zero-copy where possible)
   - Create image view and sampler

4. **Implement PNG loading path**
   - Call `load_png()`
   - Convert RGBA8 to appropriate Vulkan format
   - Create Vulkan image
   - Upload decompressed data
   - Warn in debug mode (PNG is slow!)

5. **Implement destructor (RAII)**
   - Clean up Vulkan objects in correct order
   - Destroy image view
   - Destroy image
   - Free memory
   - Destroy sampler

6. **Implement hot-reload method**
   - Check file modification time
   - If changed, reload texture
   - Recreate Vulkan objects (cleanup old, create new)
   - For CONTINUUM integration later

**Dependencies:** 
- Phase 1.1 (DDS loader)
- Phase 1.2 (PNG loader)
- Existing Vulkan helpers (`vulkan/eden_vulkan_helpers.cpp`)

**Testing:** 
- Load DDS texture, verify Vulkan objects created
- Load PNG texture, verify conversion works
- Verify cleanup on destruction

---

## Phase 1.4: Mesh Resource Class (`stdlib/mesh_resource.h`)

**Purpose:** Load mesh files (OBJ initially, glTF later) and create GPU buffers.

**Sub-tasks:**
1. **Create OBJ loader** (`stdlib/obj_loader.h`)
   - Parse OBJ file format (ASCII)
   - Extract vertices (`v` lines)
   - Extract normals (`vn` lines)
   - Extract texture coordinates (`vt` lines)
   - Extract faces (`f` lines) - handle different formats:
     - `f 1 2 3` (position only)
     - `f 1/1 2/2 3/3` (position + UV)
     - `f 1/1/1 2/2/2 3/3/3` (position + UV + normal)
   - Generate vertex/index buffers

2. **Create MeshData structure**
   ```cpp
   struct MeshData {
       std::vector<float> positions;   // Vec3 per vertex
       std::vector<float> normals;     // Vec3 per vertex
       std::vector<float> texcoords;   // Vec2 per vertex
       std::vector<uint32_t> indices;  // Index buffer
   };
   ```

3. **Create MeshResource class**
   - Member variables (vertex/index buffers, GPU memory)
   - Index count
   - Path (for hot-reload)

4. **Implement constructor**
   - Take file path
   - Detect format (`.obj`, `.gltf` - future)
   - Call OBJ loader
   - Create vertex buffer (positions, normals, UVs)
   - Create index buffer
   - Upload to GPU

5. **Implement destructor (RAII)**
   - Destroy buffers
   - Free GPU memory

**Dependencies:** None (OBJ is ASCII, no library needed)

**Testing:** 
- Load OBJ file, verify vertex/index data
- Verify GPU buffers created
- Verify cleanup works

**Note:** glTF support comes later (Phase 1.6 or separate task).

---

## Phase 1.5: Resource Template Wrapper (`stdlib/resource.h`)

**Purpose:** Generic wrapper that provides hot-reload, file watching, and RAII lifecycle for any resource type.

**Sub-tasks:**
1. **Create Resource<T> template class**
   - Generic wrapper for any resource type
   - Member: `std::unique_ptr<T>` (the actual resource)
   - Member: `std::string path` (file path)
   - Member: `std::time_t lastModified` (for file watching)

2. **Implement constructor**
   - Take file path
   - Load file modification time
   - Create resource instance: `data = std::make_unique<T>(path)`

3. **Implement accessors**
   - `T* get()` - Get raw pointer
   - `T& operator*()` - Dereference
   - `T* operator->()` - Member access

4. **Implement reload method**
   - Check file modification time
   - If changed:
     - Destroy old resource
     - Create new resource
     - Update modification time

5. **Implement file watching (for CONTINUUM)**
   - Register with file watcher system
   - Call `reload()` on file change
   - Integration with existing hot-reload infrastructure

6. **RAII lifecycle**
   - Destructor cleans up resource automatically
   - No manual memory management needed

**Dependencies:**
- Phase 1.3 (TextureResource)
- Phase 1.4 (MeshResource)
- File watching (can use existing CONTINUUM infrastructure)

**Testing:**
- Create `Resource<TextureResource>`, verify loading
- Modify texture file, verify hot-reload triggers
- Verify cleanup on destruction

**Usage Example:**
```cpp
Resource<TextureResource> texture("textures/brick.dds");
// Use texture:
auto* tex = texture.get();
// Hot-reload automatically handles file changes
```

---

## Phase 1.6: Testing & Integration

**Purpose:** Verify everything works together before moving to HEIDIC syntax.

**Sub-tasks:**
1. **Create manual test program** (`test_resources.cpp`)
   - Test DDS loading
   - Test PNG loading
   - Test TextureResource creation
   - Test MeshResource creation
   - Test Resource<T> wrapper
   - Verify Vulkan integration works

2. **Test hot-reload**
   - Load texture
   - Modify DDS file
   - Verify reload triggers
   - Verify rendering updates

3. **Integration with existing renderer**
   - Add test texture to bouncing_balls project
   - Verify rendering works
   - Measure performance (DDS vs PNG)

4. **Documentation**
   - Add comments to all classes
   - Document format support
   - Document usage patterns

---

## Summary: Phase 1 Checklist

- [ ] **1.1** DDS Loader - Parse headers, extract compressed data
- [ ] **1.2** PNG Loader - Load source files (stb_image or custom)
- [ ] **1.3** TextureResource - High-level texture class with Vulkan integration
- [ ] **1.4** MeshResource - Mesh loading (OBJ parser + GPU buffers)
- [ ] **1.5** Resource<T> - Generic wrapper with hot-reload
- [ ] **1.6** Testing - Verify everything works together

**Estimated Time:**
- 1.1 (DDS): 1-2 days
- 1.2 (PNG): 0.5 days (if using stb_image)
- 1.3 (TextureResource): 2-3 days
- 1.4 (MeshResource): 2-3 days
- 1.5 (Resource<T>): 1-2 days
- 1.6 (Testing): 1-2 days

**Total: ~1-2 weeks** for Phase 1

---

## Next: Phase 2 (HEIDIC Syntax)

Once Phase 1 is complete, we'll add HEIDIC language syntax that generates code using these C++ classes. But first, we need the foundation!

