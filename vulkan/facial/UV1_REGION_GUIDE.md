# UV1 Region Guide for Facial Animation

## Core Facial Regions for DMap Atlas

When creating UV1 in Blender, organize these regions into rectangular areas in your UV atlas:

### Essential Regions (Start with these):

1. **Brow/Forehead Region**
   - **UV Coordinates**: (0.4 - 0.7, 0.7 - 1.0)
   - **Purpose**: Brow raises, brow furrows, forehead wrinkles
   - **Vertices**: All forehead and brow vertices

2. **Lip/Mouth Region**
   - **UV Coordinates**: (0.6 - 0.9, 0.3 - 0.5)
   - **Purpose**: Smiles, frowns, lip puckering, mouth opening
   - **Vertices**: All lip vertices (upper and lower)

3. **Jaw/Chin Region**
   - **UV Coordinates**: (0.7 - 1.0, 0.0 - 0.3)
   - **Purpose**: Jaw opening/closing, chin movement
   - **Vertices**: Jaw and chin vertices

4. **Cheek Region**
   - **UV Coordinates**: (0.0 - 0.6, 0.4 - 0.7)
   - **Purpose**: Cheek puffing, smiling (cheek lift)
   - **Vertices**: Cheek vertices

### Secondary Regions (Add if needed):

5. **Nose Region**
   - **UV Coordinates**: (0.0 - 0.6, 0.7 - 1.0) or overlap with brow
   - **Purpose**: Nose wrinkling, nostril flaring
   - **Vertices**: Nose vertices

6. **Eye Region** (if you want eye-specific control)
   - **UV Coordinates**: (0.0 - 0.4, 0.0 - 0.4)
   - **Purpose**: Eye widening, squinting
   - **Vertices**: Eyelid and eye area vertices

### Safe Zone (Body/Non-face):

7. **Body/Neutral Zone**
   - **UV Coordinates**: (0.0 - 0.1, 0.0 - 0.1)
   - **Purpose**: Body vertices that shouldn't move
   - **Vertices**: All body vertices (map to this tiny corner)

## Visual Layout:

```
UV1 Atlas (1024x1024 or 512x512):
┌─────────────────────────────────────┐
│  [Nose]      [Brow]                 │  ← Top
│  (0-0.6,     (0.4-0.7,              │
│   0.7-1.0)    0.7-1.0)               │
│                                      │
│  [Cheek]     [Lip]                  │  ← Middle
│  (0-0.6,     (0.6-0.9,              │
│   0.4-0.7)    0.3-0.5)               │
│                                      │
│  [Eye]       [Jaw]                  │  ← Bottom
│  (0-0.4,     (0.7-1.0,              │
│   0.0-0.4)    0.0-0.3)               │
│                                      │
│  [Body]                              │  ← Safe corner
│  (0-0.1, 0-0.1)                      │
└─────────────────────────────────────┘
```

## Blender Workflow:

1. **Select facial feature vertices** (e.g., all brow vertices)
2. **Unwrap to UV1** (second UV map)
3. **Move/Scale the island** to the designated region
4. **Repeat for each region**
5. **Select all body vertices** → Move to (0.1, 0.1) safe zone

## Quick Start (Minimum Viable):

If you want to start simple, just create these 3 regions:

1. **Brow** → (0.4-0.7, 0.7-1.0)
2. **Lips** → (0.6-0.9, 0.3-0.5)
3. **Jaw** → (0.7-1.0, 0.0-0.3)
4. **Body** → (0.1, 0.1) - tiny corner

This gives you the basics for facial expressions!

## Tips:

- **Keep regions rectangular** - easier to paint in image editors
- **Leave some padding** between regions - prevents bleeding
- **Use consistent sizes** - similar-sized features get similar-sized regions
- **Test in EaSE** - load model and check UV1 tab to verify regions are where you expect

