# How to Create DMaps

## What is a DMap?

A DMap (Displacement Map) is an RGB texture where each pixel encodes a displacement vector:
- **Red channel** = X displacement
- **Green channel** = Y displacement  
- **Blue channel** = Z displacement

**Color to displacement mapping:**
- `0` (black) = -1.0 (move in negative direction)
- `128` (gray) = 0.0 (no movement)
- `255` (white) = +1.0 (move in positive direction)

## Quick Reference Colors:

| Color (RGB)         | Displacement (XYZ)      | Effect                |
|---------------------|-------------------------|----------------------|
| (128, 128, 128)     | (0, 0, 0)              | No movement          |
| (255, 128, 128)     | (+1, 0, 0)             | Move RIGHT (+X)      |
| (0, 128, 128)       | (-1, 0, 0)             | Move LEFT (-X)       |
| (128, 255, 128)     | (0, +1, 0)             | Move UP (+Y)         |
| (128, 0, 128)       | (0, -1, 0)             | Move DOWN (-Y)       |
| (128, 128, 255)     | (0, 0, +1)             | Move FORWARD (+Z)    |
| (128, 128, 0)       | (0, 0, -1)             | Move BACKWARD (-Z)   |
| (255, 255, 128)     | (+1, +1, 0)            | Move UP-RIGHT        |

## Method 1: Image Editor (Photoshop, GIMP, Paint.NET)

### Step 1: Create Base Image
1. Create a 512x512 or 1024x1024 image
2. Fill with **RGB (128, 128, 128)** — neutral gray, no displacement

### Step 2: Paint Displacement Regions
Based on your UV1 layout, paint colors in the regions:

**Example "Jaw Open" DMap:**
```
┌─────────────────────────────────────┐
│  Gray (128,128,128) - Forehead      │  ← No movement
│                                      │
│  Gray (128,128,128) - Cheeks        │  ← No movement
│                                      │
│  Dark Green (128,64,128) - Jaw      │  ← Move DOWN (-Y)
│                                      │
└─────────────────────────────────────┘
```

**Example "Smile" DMap:**
```
┌─────────────────────────────────────┐
│  Gray - Forehead                    │  ← No movement
│                                      │
│  Light Red (192,160,128) - Cheeks   │  ← Move right+up
│                                      │
│  Light Red (192,160,128) - Lips     │  ← Move right+up (corners up)
│                                      │
└─────────────────────────────────────┘
```

### Step 3: Save as PNG
Save as PNG (lossless) — JPG compression will corrupt the displacement data!

### Step 4: Load in EaSE
Use the file loader to load your DMap texture.

## Method 2: Blender (Best Quality)

### Step 1: Create Base Mesh
1. Have your character mesh with UV1 layout ready

### Step 2: Sculpt Extreme Pose
1. Duplicate the mesh
2. Sculpt the extreme pose (e.g., jaw fully open)
3. Keep vertex count IDENTICAL to original

### Step 3: Bake Displacement
Use this Python script in Blender:

```python
import bpy
import bmesh
import numpy as np

# Settings
base_obj_name = "Head_Base"      # Your neutral pose mesh
sculpt_obj_name = "Head_JawOpen" # Your sculpted pose mesh
output_path = "//dmap_jawopen.png"
resolution = 1024

# Get objects
base_obj = bpy.data.objects[base_obj_name]
sculpt_obj = bpy.data.objects[sculpt_obj_name]

# Get meshes
base_mesh = base_obj.data
sculpt_mesh = sculpt_obj.data

# Create image
img = bpy.data.images.new("DMap", resolution, resolution, alpha=False)
pixels = np.full(resolution * resolution * 4, 0.5, dtype=np.float32)  # Start with gray

# Get UV1 layer (second UV map)
uv_layer = base_mesh.uv_layers[1] if len(base_mesh.uv_layers) > 1 else base_mesh.uv_layers[0]

# Calculate displacement for each vertex
for poly in base_mesh.polygons:
    for loop_idx in poly.loop_indices:
        vert_idx = base_mesh.loops[loop_idx].vertex_index
        uv = uv_layer.data[loop_idx].uv
        
        # Get displacement
        base_pos = base_mesh.vertices[vert_idx].co
        sculpt_pos = sculpt_mesh.vertices[vert_idx].co
        delta = sculpt_pos - base_pos
        
        # Normalize to [-1, 1] then to [0, 1]
        max_disp = 0.1  # Maximum expected displacement in Blender units
        disp_normalized = delta / max_disp
        disp_normalized = np.clip(disp_normalized, -1, 1)
        disp_color = (disp_normalized + 1) / 2  # Map to 0-1
        
        # Write to image
        x = int(uv[0] * (resolution - 1))
        y = int(uv[1] * (resolution - 1))
        idx = (y * resolution + x) * 4
        pixels[idx:idx+3] = [disp_color[0], disp_color[1], disp_color[2]]
        pixels[idx+3] = 1.0  # Alpha

img.pixels[:] = pixels.tolist()
img.filepath_raw = bpy.path.abspath(output_path)
img.file_format = 'PNG'
img.save()
print(f"DMap saved to {output_path}")
```

## Method 3: Procedural Generation (In EaSE)

Use the built-in "Create Test DMap" buttons for simple tests:
- **Upward Push**: All vertices move up when slider is applied
- **Forward Push**: All vertices move forward when slider is applied

For real facial animation, you need region-specific DMaps.

## Tips for Good DMaps:

1. **Start with neutral gray** — only paint where you want movement
2. **Use soft brushes** — hard edges create harsh deformations
3. **Keep values moderate** — don't use pure black/white (too extreme)
4. **Match UV1 layout** — your DMap regions must align with UV1 islands
5. **Test incrementally** — create simple DMaps first, refine later

## Common Mistakes:

❌ Using JPG format (compression corrupts data)
❌ Painting outside UV1 regions (wasted effort)
❌ Using extreme values (causes mesh explosion)
❌ Forgetting to save as RGB (not grayscale)

## Summary Workflow:

1. Set up UV1 in Blender (region-based layout)
2. Export GLB with UV1
3. Create DMaps (image editor or Blender script)
4. Load DMaps in EaSE
5. Connect sliders to DMaps
6. Animate!

