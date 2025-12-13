# PNG Loader Test Project

This project tests the PNG texture loader by displaying a PNG image on a fullscreen quad.

## Setup

1. **Add a test PNG image:**
   - Place a PNG file named `test.png` in the `textures/` folder
   - Any PNG image will work (test image, logo, etc.)

2. **Build and Run:**
   - Open this project in SCRIBE editor
   - Click the white "Run" button (▶)
   - You should see your PNG texture displayed on a fullscreen quad!

## Project Structure

```
png_test/
├── png_test.hd          # HEIDIC script (main entry point)
├── shaders/
│   ├── quad.vert        # Fullscreen quad vertex shader
│   └── quad.frag        # Texture sampling fragment shader
└── textures/
    └── test.png         # Your PNG texture (add this!)
```

## Notes

- PNG loader uses `stb_image.h` for loading
- PNG files are loaded as uncompressed RGBA8 format
- For production, convert PNG → DDS at build time for better performance
- The quad shaders are automatically compiled during build (no manual step needed)

