# Direct 3D Painting Workflow for DMap Creation

## The Idea

Instead of manually creating UV1 layouts and painting DMap textures in Blender, you can:

1. **Load model in EaSE** (with just UV0 for skin texture)
2. **Paint directly on the 3D model** - like sculpting or vertex painting
3. **System auto-generates UV1** and bakes your painting to DMap textures
4. **Save DMap textures** automatically

## How It Would Work

### Step 1: Load Model
- Load your head mesh (only needs UV0 for skin texture)
- System detects it's a facial mesh (or you mark it as such)

### Step 2: Enter Paint Mode
- Click "Facial Paint Mode" button in UI
- Model becomes paintable (like a 3D sculpting tool)

### Step 3: Paint Displacement
- **Brush tool**: Paint on the model where you want displacement
- **Color = Direction**:
  - Red brush = move vertices RIGHT (+X)
  - Green brush = move vertices UP (+Y)  
  - Blue brush = move vertices FORWARD (+Z)
  - Or use a single brush that records displacement amount
- **Brush size**: Adjustable (affects how many vertices)
- **Strength**: How much displacement (0.0 to 1.0)

### Step 4: Auto-Generate UV1
- System automatically creates UV1 layout:
  - Projects model to a simple atlas (like a cube map projection)
  - Or uses a region-based layout (face, jaw, lips, etc.)
  - Maps each painted vertex to UV1 coordinates

### Step 5: Bake to DMap
- When you're done painting, click "Bake DMap"
- System:
  - Samples all painted vertices
  - Maps them to UV1 coordinates
  - Creates RGB texture (displacement → RGB)
  - Saves as PNG

### Step 6: Save
- DMap texture is saved automatically
- Can create multiple DMaps (jaw open, smile, etc.)
- Each DMap is linked to a slider

## Implementation Plan

### What We Need:

1. **Raycasting System** (already have `vulkan/utils/raycast.cpp`)
   - Click on model → find which face/vertex is hit
   - Get 3D position and normal

2. **Vertex Painting Buffer**
   - Store displacement per vertex: `vec3 displacement[MAX_VERTICES]`
   - Initialize to zero (neutral)
   - Update when painting

3. **Paint Brush**
   - Mouse position → raycast → find vertices in brush radius
   - Apply displacement based on brush color/strength
   - Visualize brush as a sphere on the model

4. **UV1 Auto-Generation**
   - Simple projection: project vertices to a 2D atlas
   - Or use existing UV0 and create UV1 as a copy (can be refined later)
   - Store UV1 coordinates per vertex

5. **DMap Baking**
   - For each vertex:
     - Get UV1 coordinates
     - Get displacement vector
     - Convert to RGB (normalize to -1..1, map to 0..255)
     - Write to texture at UV1 position

6. **UI Controls**
   - Paint mode toggle
   - Brush size slider
   - Brush strength slider
   - Color picker (R/G/B for displacement direction)
   - "Bake DMap" button
   - "Save DMap" button
   - "Clear Painting" button

## Benefits

✅ **No Blender needed** - everything in EaSE
✅ **Visual feedback** - see displacement in real-time
✅ **Intuitive** - paint like sculpting
✅ **Fast iteration** - test immediately
✅ **Auto UV1** - no manual UV layout work

## Alternative: Real-Time Displacement Preview

Even better - while painting, we could:
- Apply displacement directly to vertices (in CPU)
- Update mesh buffer
- See the result immediately
- Then bake to DMap for permanent storage

This would be like a "sculpting mode" that creates DMaps!

