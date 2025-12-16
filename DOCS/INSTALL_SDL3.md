# Installing SDL3 for EDEN Engine

## Quick Installation Guide

### Option 1: SDL3 (Recommended - Latest Version)

1. Download SDL3 development libraries:
   - Visit: https://github.com/libsdl-org/SDL/releases
   - Download the latest SDL3 release for Windows (MinGW or Visual C++)

2. Extract to `C:\SDL3`

3. Verify installation:
   - Check that `C:\SDL3\include\SDL3\SDL.h` exists
   - Check that `C:\SDL3\lib\x64\libSDL3.a` (or `SDL3.lib` for VC) exists

### Option 2: SDL2 (Fallback - Still Supported)

If you prefer SDL2 or SDL3 is not available:

1. Download SDL2 development libraries:
   - **MinGW version**: https://github.com/libsdl-org/SDL/releases/download/release-2.30.0/SDL2-devel-2.30.0-mingw.tar.gz
   - **Visual C++ version**: https://www.libsdl.org/release/SDL2-devel-2.30.0-VC.zip

2. Extract to `C:\SDL2`

3. Verify installation:
   - Check that `C:\SDL2\include\SDL2\SDL.h` exists
   - Check that `C:\SDL2\lib\x64\libSDL2.a` (or `SDL2.lib` for VC) exists

### Environment Variables

If you install to a different location, set environment variables:

- **SDL3**: `set SDL3_PATH=C:\Your\SDL3\Path`
- **SDL2**: `set SDL2_PATH=C:\Your\SDL2\Path`

## Build System Behavior

The build system will:
1. **First check for SDL3** at `C:\SDL3` or `SDL3_PATH`
2. **Fall back to SDL2** at `C:\SDL2` or `SDL2_PATH` if SDL3 is not found
3. Automatically use the correct ImGui backends (`imgui_impl_sdl3` or `imgui_impl_sdl2`)

## Directory Structure

### SDL3:
```
C:\SDL3\
  ├── include\
  │   └── SDL3\
  │       ├── SDL.h
  │       └── ... (other headers)
  ├── lib\
  │   ├── x64\
  │   │   └── libSDL3.a (or SDL3.lib)
  │   └── x86\
  └── bin\
      └── x64\
          └── SDL3.dll
```

### SDL2:
```
C:\SDL2\
  ├── include\
  │   └── SDL2\
  │       ├── SDL.h
  │       └── ... (other headers)
  ├── lib\
  │   ├── x64\
  │   │   ├── libSDL2.a (or SDL2.lib)
  │   │   └── libSDL2main.a (or SDL2main.lib)
  │   └── x86\
  └── bin\
      └── x64\
          └── SDL2.dll
```

## Verification

After installation, the build system will automatically detect SDL3 or SDL2. Check the build log for:
- `Using SDL3 include: ...` (if SDL3 is found)
- `Using SDL2 include: ...` (if SDL2 is found as fallback)

## Troubleshooting

- **"SDL3/SDL2 not found"**: Make sure SDL3 is at `C:\SDL3` or SDL2 is at `C:\SDL2`, or set the environment variable
- **"Cannot find SDL.h"**: Check that `C:\SDL3\include\SDL3\SDL.h` (SDL3) or `C:\SDL2\include\SDL2\SDL.h` (SDL2) exists
- **Link errors**: Make sure the library files exist in `lib\x64\` directory

