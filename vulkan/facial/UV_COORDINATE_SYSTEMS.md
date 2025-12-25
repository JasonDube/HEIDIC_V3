# UV Coordinate Systems Explained

## The Problem

Different systems use different coordinate conventions, which can cause confusion and bugs.

## Coordinate System Comparison

### Blender's UV Editor:
```
U: 0 = left,  1 = right  (standard, matches everyone)
V: 0 = bottom, 1 = top  (THIS IS THE KEY DIFFERENCE!)
```

### Screen/ImGui Coordinates:
```
X: 0 = left,  width = right  (standard)
Y: 0 = top,   height = bottom  (Y increases downward!)
```

### Standard Graphics/Texture Coordinates:
```
U: 0 = left,  1 = right  (standard)
V: 0 = top,   1 = bottom  (V increases downward - opposite of Blender!)
```

## The Mismatch

**Blender**: V=0 at bottom, V=1 at top (V increases upward)
**Screen**: Y=0 at top, Y=height at bottom (Y increases downward)
**Standard**: V=0 at top, V=1 at bottom (V increases downward)

## Conversion Formula

To convert Blender UV to Screen coordinates:
```
screenX = uv.u * canvasWidth
screenY = (1.0 - uv.v) * canvasHeight  // Flip V because Blender V increases upward
```

To convert Screen coordinates back to Blender UV:
```
u = screenX / canvasWidth
v = 1.0 - (screenY / canvasHeight)  // Flip V back
```

## Why This Matters

1. **Visual Display**: Need to flip V when drawing to match Blender's visual
2. **Coordinate Values**: Need to flip V when calculating from mouse to get Blender's actual UV values
3. **DMap Sampling**: Shader uses actual UV values (no flip needed in shader - it samples texture directly)

## Common Mistakes

❌ **Wrong**: Using V directly without flip → Image appears upside down
❌ **Wrong**: Flipping V twice → Image appears upside down
✅ **Correct**: Flip V once when converting between Blender UV and Screen coordinates

## Testing

To verify coordinate system:
1. In Blender, select a vertex at the **top** of the head
2. Check its V coordinate in UV editor
3. If V ≈ 1.0 → Blender V=0 at bottom, V=1 at top ✓
4. If V ≈ 0.0 → Blender V=0 at top, V=1 at bottom (unlikely but possible)

## Current Implementation (CORRECTED)

**Key Insight**: 
- Blender stores V with V=0 at bottom, V=1 at top
- Screen Y has Y=0 at top, Y=height at bottom
- When we use V directly for display: V=0 → top of screen (opposite to Blender's visual)
- But this makes the IMAGE match Blender visually!
- When we flip V for display: V=0 → bottom of screen (matches Blender's convention)
- But this makes the IMAGE flipped!

**Solution**:
- **Display**: Use V directly (no flip) → `screenY = uv.v * canvasHeight`
  - This makes the image match Blender visually ✓
- **Coordinates**: Flip V when calculating from mouse → `v = 1.0 - (mouseY / canvasHeight)`
  - This gives Blender's actual V values (V=0 at bottom) ✓

**Why this works**:
- Blender's stored V values: V=0 at bottom, V=1 at top
- Screen Y: Y=0 at top, Y=height at bottom
- Using V directly maps: V=0 → Y=0 (top), V=1 → Y=height (bottom)
- This is inverted from Blender's visual, but matches the image orientation
- When calculating from mouse, we flip to get Blender's actual stored values

