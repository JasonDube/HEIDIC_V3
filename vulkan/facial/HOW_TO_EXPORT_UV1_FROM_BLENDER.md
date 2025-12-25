# How to Export UV1 from Blender to GLB

## The Process:

### 1. **In Blender - Create UV1 Channel**

1. Select your mesh
2. Go to **Mesh Data Properties** (green triangle icon)
3. Under **UV Maps**, you'll see your first UV map (usually called "UVMap")
4. Click the **+** button to add a new UV map
5. Name it "UVMap.001" or "UV1" (this becomes TEXCOORD_1 in GLB)

### 2. **In Blender - Set Active UV Map**

1. In **Edit Mode**, go to **UV Editor**
2. In the UV Editor header, you'll see a dropdown showing active UV map
3. Select your new UV map (UVMap.001)
4. This is now the active UV map you'll be editing

### 3. **In Blender - Create Your UV1 Layout**

1. Select faces for a feature (e.g., all lip faces)
2. Press **U** → **Unwrap** (or use Smart UV Project, etc.)
3. This creates an island for that feature
4. Manually move/scale the island to your designated region (e.g., 0.6-0.9, 0.3-0.5)
5. Repeat for each feature (brows, nose, jaw, etc.)
6. For body faces: Select them → Set UV1 to (0.1, 0.1)

### 4. **In Blender - Export to GLB**

1. File → Export → glTF 2.0 (.glb/.gltf)
2. In export settings:
   - **Format**: GLB (binary)
   - **Include**: Check "UVs" (this exports both UV0 and UV1)
   - **Transform**: Your choice
3. Click **Export**

**Important:** Blender automatically exports ALL UV maps as TEXCOORD_0, TEXCOORD_1, etc. in the GLB file!

## How EaSE Reads It:

### GLB File Structure:
```
GLB File:
├─ Meshes
│  └─ Primitive
│     └─ Attributes:
│        ├─ POSITION (required)
│        ├─ NORMAL (optional)
│        ├─ TEXCOORD_0 (UV0 - first UV map)
│        ├─ TEXCOORD_1 (UV1 - second UV map) ← This is what we read!
│        └─ COLOR (optional)
```

### Our Loader Code:
```cpp
// In glb_loader.cpp, we look for TEXCOORD_1:
for (size_t ai = 0; ai < prim.attributes_count; ++ai) {
    cgltf_attribute& attr = prim.attributes[ai];
    if (attr.type == cgltf_attribute_type_texcoord) {
        if (attr.index == 0) uvAccessor = attr.data;      // TEXCOORD_0 (UV0)
        else if (attr.index == 1) uv1Accessor = attr.data; // TEXCOORD_1 (UV1)
    }
}

// Then we read it:
if (uv1Accessor) {
    cgltf_accessor_read_float(uv1Accessor, vi, &v.texCoord1.x, 2);
    mesh.hasUV1 = true;  // Mark that we have UV1 data
}
```

## What Happens When You Load:

1. **EaSE loads GLB file**
2. **Checks for TEXCOORD_1 attribute**
3. **If found:**
   - Reads UV1 coordinates for each vertex
   - Sets `m_mesh.hasUV1 = true`
   - Stores in `v.texCoord1`
4. **If not found:**
   - Sets `m_mesh.hasUV1 = false`
   - Uses default UV1 = (0.5, 0.5) when facial animation is enabled

## Console Output:

When you load a GLB with UV1, you'll see:
```
[GLB] Mesh 'Head' has TEXCOORD_1 (UV1 for DMap)
```

If UV1 is missing:
```
[GLB] Mesh 'Head' has no TEXCOORD_1 - will use default UV1
```

## Quick Checklist:

✅ Create second UV map in Blender (UVMap.001)
✅ Set it as active in UV Editor
✅ Unwrap facial features into islands
✅ Pack islands into designated regions
✅ Set body faces to (0.1, 0.1)
✅ Export GLB with "UVs" checked
✅ EaSE automatically reads TEXCOORD_1 from GLB

## Troubleshooting:

**Q: UV1 not showing up in EaSE?**
- Check that you exported with "UVs" checked
- Check console for "[GLB] Mesh has TEXCOORD_1" message
- Verify in Blender that UVMap.001 actually has data (not all zeros)

**Q: How do I verify UV1 in Blender?**
- In UV Editor, switch between UVMap and UVMap.001
- You should see different layouts
- UVMap.001 should have your packed islands

**Q: Can I have more than 2 UV maps?**
- Yes! GLB supports TEXCOORD_0, TEXCOORD_1, TEXCOORD_2, etc.
- EaSE currently only reads TEXCOORD_0 (UV0) and TEXCOORD_1 (UV1)
- Additional UV maps are ignored

