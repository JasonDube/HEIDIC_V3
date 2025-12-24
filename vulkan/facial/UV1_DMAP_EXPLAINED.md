# UV1 and DMap Explained Simply

## The Confusion

You asked: "UV1 is the albedo, color map right?"

**No!** Here's the breakdown:

## Three Separate Things

### 1. UV0 (Standard UV Channel)
- **Purpose**: Maps your mesh to the **albedo/color texture** (skin texture)
- **What it does**: Tells the shader "this vertex uses pixel (u, v) from the skin texture"
- **Example**: Face vertex at (0.5, 0.5) in UV0 → samples center of skin texture

### 2. UV1 (Second UV Channel)
- **Purpose**: Maps your mesh to the **DMap texture** (displacement map)
- **What it does**: Tells the shader "this vertex uses pixel (u, v) from the DMap texture"
- **Key difference**: UV1 layout is **organized by facial regions**, not by how the mesh looks
- **Example**: Jaw vertex at (0.8, 0.1) in UV1 → samples jaw region of DMap

### 3. DMap Texture (Displacement Map)
- **Purpose**: Contains **displacement vectors**, not colors
- **What it contains**: RGB values that encode "move vertex X units right, Y units up, Z units forward"
- **Not a color texture**: It's a data texture (like a normal map, but for position)

## Visual Example

```
┌─────────────────────────────────────────────────────────┐
│                    YOUR HEAD MESH                        │
│                                                          │
│  Each vertex has:                                       │
│  - Position (x, y, z)                                    │
│  - UV0 coordinates → points to SKIN TEXTURE             │
│  - UV1 coordinates → points to DMAP TEXTURE            │
└─────────────────────────────────────────────────────────┘

┌──────────────────┐         ┌──────────────────┐
│  SKIN TEXTURE    │         │   DMAP TEXTURE   │
│  (UV0)           │         │   (UV1)           │
│                  │         │                  │
│  [Face colors]   │         │  [Displacement]  │
│  RGB = skin      │         │  R = X movement  │
│                  │         │  G = Y movement  │
│                  │         │  B = Z movement  │
└──────────────────┘         └──────────────────┘
```

## How It Works Together

1. **Vertex shader receives:**
   - Vertex position: `(0.0, 1.0, 0.0)`
   - UV0: `(0.5, 0.5)` → samples skin texture → gets skin color
   - UV1: `(0.8, 0.1)` → samples DMap texture → gets displacement vector

2. **DMap sample returns:**
   - RGB = `(200, 128, 180)` 
   - Converted to displacement: `(+0.56, 0.0, +0.41)` units

3. **Vertex is displaced:**
   - New position = `(0.0, 1.0, 0.0) + (0.56, 0.0, 0.41) = (0.56, 1.0, 0.41)`

4. **Fragment shader:**
   - Uses UV0 to sample skin texture for final color

## Why Two UV Channels?

**UV0** is optimized for **texture quality** (minimize seams, maximize detail)
**UV1** is optimized for **facial regions** (jaw, lips, brows in separate areas)

They can be completely different layouts!

## Creating UV1 Layout

Think of UV1 as a **control panel** for your face:

```
UV1 Layout (like a control panel):
┌─────────────────────────────────┐
│  [Brow Controls]                │  ← Paint brow expressions here
│                                 │
│  [Cheek/Nose Controls]          │  ← Paint cheek puffs here
│                                 │
│  [Lip Controls]                 │  ← Paint smiles/frowns here
│                                 │
│  [Jaw Controls]                 │  ← Paint jaw open/close here
└─────────────────────────────────┘
```

When you paint a DMap, you're painting **displacement instructions** in these regions, not colors.

## Quick Start Workflow

1. **In Blender:**
   - Create head mesh
   - Unwrap for UV0 (skin texture)
   - Create second UV map (UV1) with region-based layout
   - Sculpt extreme pose (e.g., jaw wide open)
   - Use script to bake displacement → DMap texture

2. **Export GLB:**
   - Both UV0 and UV1 are automatically exported
   - DMap texture is saved separately (not embedded)

3. **In EaSE:**
   - Load GLB (gets UV0 + UV1)
   - Load skin texture (uses UV0)
   - Load DMap texture (uses UV1)
   - Control slider → vertices move!

## Common Mistake

❌ **Wrong**: "UV1 is my color texture"
✅ **Right**: "UV1 is a second coordinate set for sampling the DMap texture"

The DMap itself is not a color texture - it's a **data texture** that encodes movement.

