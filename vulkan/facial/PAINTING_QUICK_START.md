# Quick Start: Painting DMaps in EaSE

## The New Workflow (Much Easier!)

You asked: "Why can't we load the model in EaSE then paint directly on it?"

**Answer: You can now!** Here's how:

## Step-by-Step

### 1. Load Your Model
- Load any head mesh (OBJ or GLB) in EaSE
- Only needs UV0 (standard texture coordinates)
- **No UV1 needed** - we'll auto-generate it!

### 2. Enter Paint Mode
- Open the **"Facial Painting (Create DMaps)"** panel in the UI
- Click **"Enable Paint Mode"**
- Click **"Initialize Painter"** (this creates an editable copy of your mesh)

### 3. Paint Displacement
- **Click on the 3D model** where you want displacement
- The brush will paint displacement at that location
- Adjust:
  - **Brush Size**: How big the brush is (0.01 to 0.5 world units)
  - **Brush Strength**: How much displacement (0.0 to 1.0)
  - **Direction**: Which way to move vertices (X, Y, Z)

### 4. Create Different Expressions
- Paint jaw open: Click on jaw, direction = (0, -0.5, 1) for forward+down
- Paint smile: Click on lip corners, direction = (1, 0.3, 0) for outward+up
- Paint brow raise: Click on brows, direction = (0, 1, 0) for up

### 5. Bake to DMap
- When done painting, click **"Bake DMap Texture"**
- Enter a filename (e.g., "jaw_open_dmap.png")
- Click **"Save"**
- The system automatically:
  - Generates UV1 coordinates (region-based atlas)
  - Converts your painted displacement to RGB texture
  - Saves as PNG

### 6. Use the DMap
- Load the DMap in Facial Animation panel:
  - `m_facial.loadDMap("jaw_open_dmap.png", "jawOpen", 0)`
- Control with slider:
  - `m_facial.setSliderWeight(0, 0.5f)`  // 50% jaw open

## How It Works

1. **You paint** → System records displacement per vertex
2. **Auto UV1** → System creates UV1 layout automatically
3. **Bake** → System maps vertex displacement to UV1 → RGB texture
4. **Done!** → DMap texture ready to use

## Tips

- **Start small**: Use small brush size (0.05) for detail
- **Test direction**: Use "Forward (+Z)" button to test jaw movement
- **Undo**: Click "Undo" if you make a mistake
- **Clear**: Click "Clear Painting" to start over
- **Multiple DMaps**: Create one DMap per expression (jaw, smile, frown, etc.)

## What Gets Created

- **UV1 coordinates**: Auto-generated, region-based layout
- **DMap texture**: RGB image where R/G/B = X/Y/Z displacement
- **Mesh preview**: See displacement in real-time (when mesh update is implemented)

## No Blender Needed!

This workflow is **much simpler** than the Blender approach:
- ❌ No manual UV1 layout
- ❌ No sculpting in Blender
- ❌ No Python scripts
- ✅ Paint directly in EaSE
- ✅ See results immediately
- ✅ Auto-generate everything

