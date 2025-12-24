# DMap Facial Animation Guide

## Overview

The DMap system uses **two separate UV channels**:
- **UV0**: Standard unwrap for your skin/albedo texture
- **UV1**: Specialized layout for sampling displacement maps (DMap)

## Creating UV1 in Blender

### Step 1: Create Base Head Mesh
1. Model your head with good topology (quads, clean loops around eyes/mouth)
2. Unwrap normally for UV0 (this is your skin texture UV)

### Step 2: Create UV1 Layout
UV1 should be organized into **regions** that correspond to facial features:

```
UV1 Layout (Right half only, mirrored):
┌─────────────────────────────────────┐
│  Brows (0.4-0.7, 0.7-1.0)          │
│                                     │
│  Cheeks/Nose (0.0-0.6, 0.4-0.7)    │
│                                     │
│  Lips (0.6-0.9, 0.3-0.5)            │
│                                     │
│  Jaw/Chin (0.7-1.0, 0.0-0.3)        │
└─────────────────────────────────────┘
```

**In Blender:**
1. Select your head mesh
2. Go to **UV Editing** workspace
3. In **Properties > Mesh Data > UV Maps**, click **+** to add a new UV map
4. Name it "UVMap.001" or "DMap_UV" (this becomes UV1)
5. Select all faces and unwrap with **Unwrap** (or **Smart UV Project**)
6. Manually arrange UV islands to match the region layout above
   - Right half of face only (left side will be mirrored in shader)
   - Keep regions organized and non-overlapping

### Step 3: Export UV1 Template
1. In UV Editor, set the active UV map to your UV1
2. **UV > Export UV Layout** → Save as `uv1_template.png`
3. Use this as a guide for painting DMap textures

## Creating DMap Textures

### What is a DMap?
A DMap is an **RGB image** where each pixel encodes a **displacement vector**:
- **Red channel** = X displacement (left = 0, center = 128, right = 255)
- **Green channel** = Y displacement (down = 0, center = 128, up = 255)
- **Blue channel** = Z displacement (back = 0, center = 128, forward = 255)

**Value mapping:**
- `0` = -1.0 (maximum negative displacement)
- `128` = 0.0 (no displacement, neutral)
- `255` = +1.0 (maximum positive displacement)

### Creating a DMap in Blender

#### Method 1: Sculpting + Baking (Recommended)

1. **Sculpt the extreme pose:**
   - Start with your base head mesh
   - Enter **Sculpt Mode**
   - Sculpt the extreme expression (e.g., wide open jaw, big smile)
   - Use **Grab**, **Inflate**, **Smooth** brushes

2. **Bake displacement:**
   - Duplicate your base mesh (keep original)
   - Apply sculpt to duplicate
   - Select base mesh → **Modifiers > Data Transfer**
     - Source: Sculpted mesh
     - Face Corner Data > Nearest Face Interpolated
     - UV Layer: UV1 (your DMap UV map)
   - Apply modifier

3. **Export displacement:**
   - Use a Python script or addon to:
     - Sample UV1 coordinates
     - Calculate delta = sculpted_pos - base_pos
     - Bake to RGB texture (XYZ → RGB)

#### Method 2: Manual Painting (Simpler for testing)

1. Open your UV1 template in an image editor (GIMP, Photoshop, etc.)
2. Create a new RGB image (1024x1024 or 2048x2048)
3. Paint displacement:
   - **Red channel**: Paint white where you want vertices to move RIGHT (+X)
   - **Green channel**: Paint white where you want vertices to move UP (+Y)
   - **Blue channel**: Paint white where you want vertices to move FORWARD (+Z)
   - Paint black for opposite direction, gray (128) for no displacement

**Example: "Jaw Open" DMap:**
- Jaw/chin region: Blue channel = white (jaw moves forward/down)
- Rest of face: All channels = gray (128) = no displacement

### DMap Examples

**Jaw Open:**
- Jaw region (UV1: 0.7-1.0, 0.0-0.3): Blue = 255 (forward), Green = 0 (down)
- Rest: All channels = 128 (neutral)

**Smile:**
- Lip corners (UV1: 0.6-0.9, 0.3-0.5): Red = 255 (corners pull right/left), Green = 200 (slight up)
- Cheeks (UV1: 0.0-0.6, 0.4-0.7): Green = 220 (cheeks puff up slightly)
- Rest: All channels = 128

**Brow Raise:**
- Brow region (UV1: 0.4-0.7, 0.7-1.0): Green = 255 (brows move up)
- Rest: All channels = 128

## Exporting to GLB with Both UV Channels

### In Blender:

1. **Ensure both UV maps exist:**
   - UV0: Your standard texture UV (should already exist)
   - UV1: Your DMap UV (created in Step 2 above)

2. **Set up materials:**
   - Base material with albedo texture (uses UV0)
   - DMap texture (uses UV1) - you'll load this separately in the engine

3. **Export as GLB:**
   - **File > Export > glTF 2.0 (.glb/.gltf)**
   - In export settings:
     - ✅ **Include UVs** (both UV0 and UV1 will be exported)
     - ✅ **Include Normals**
     - ✅ **Include Vertex Colors** (optional)
   - Click **Export**

### Verifying Export:

The GLB loader will automatically:
- Extract `TEXCOORD_0` → `texCoord` (UV0)
- Extract `TEXCOORD_1` → `texCoord1` (UV1)
- Check `mesh.hasUV1` to see if UV1 was found

## Loading in EaSE

```cpp
// 1. Load GLB (automatically extracts UV0 and UV1)
loadGLB("head.glb");

// 2. Load DMap texture
m_facial.loadDMap("jaw_open_dmap.png", "jawOpen", 0);  // Slider 0

// 3. Control at runtime
m_facial.setSliderWeight(0, 0.5f);  // 50% jaw open
```

## Quick Test Setup

If you don't have a full head mesh yet, you can test with a simple cube:

1. **Create cube in Blender**
2. **Add UV0**: Standard unwrap
3. **Add UV1**: Create second UV map, unwrap differently (e.g., all faces in one region)
4. **Create simple DMap**: Paint one face region white in blue channel (forward displacement)
5. **Export GLB**
6. **Load in EaSE** and test slider

## Tips

- **Start simple**: One DMap (jaw open) with one slider
- **DMap resolution**: 1024x1024 is fine for testing, 2048x2048 for production
- **UV1 layout**: Keep it organized - you'll be painting multiple DMaps using the same UV1 layout
- **Mirroring**: The shader automatically mirrors UV1.x < 0.5, so only paint right half
- **Displacement strength**: Start with small values (0.01-0.05 world units) and adjust

