# OBJ Loader Test Project

This project tests the **MeshResource** class - loads OBJ files and renders 3D meshes with support for textured OBJs.

## What This Tests

✅ **OBJ Parsing** - Loads vertices, normals, and texture coordinates  
✅ **Textured OBJs** - Supports UV coordinates for texture mapping  
✅ **Vulkan Buffers** - Creates vertex/index buffers on GPU  
✅ **3D Rendering** - Renders meshes with lighting and textures

## Setup

1. **Add a test OBJ file:**
   - A simple `cube.obj` is included in `models/` folder
   - You can replace it with any OBJ file
   - Supports textured OBJs (with `vt` lines and UV coordinates in faces)

2. **Add a texture (optional but recommended for textured OBJs):**
   - Place a texture file in `textures/` folder (DDS or PNG)
   - Update `obj_test.hd` to pass the texture path:
     ```heidic
     heidic_init_renderer_obj_mesh(window, "models/cube.obj", "textures/my_texture.dds")
     ```
   - Pass `""` (empty string) for no texture (uses white dummy texture)
   - Works with DDS or PNG (via TextureResource)

3. **Build and Run:**
   - Open this project in SCRIBE editor
   - Click the white "Run" button (▶)
   - You should see a 3D mesh rendered!

## OBJ File Format Support

The loader supports:
- `v` - Vertices (position)
- `vn` - Normals
- `vt` - Texture coordinates (UVs) - **for textured OBJs**
- `f` - Faces with formats:
  - `f 1 2 3` (position only)
  - `f 1/1 2/2 3/3` (position + UV) - **textured OBJ**
  - `f 1/1/1 2/2/2 3/3/3` (position + UV + normal) - **full textured OBJ**

## Example OBJ Files

You can test with:
- Simple cube (included)
- Any OBJ file from online (Blender exports, etc.)
- Textured OBJs (with UV coordinates)

## Next Steps

Once this works, we can:
- Add texture support (combine MeshResource + TextureResource)
- Add material support (MTL files)
- Support glTF format (Phase 1.6)

