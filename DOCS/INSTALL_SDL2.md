# Installing SDL2 for EDEN Engine

## Quick Installation Guide

### Option 1: Automatic Installation (Recommended)

1. Download SDL2 development libraries for Windows:
   - **MinGW version**: https://github.com/libsdl-org/SDL/releases/download/release-2.30.0/SDL2-devel-2.30.0-mingw.tar.gz
   - **Visual C++ version**: https://www.libsdl.org/release/SDL2-devel-2.30.0-VC.zip

2. Extract the downloaded file to `C:\SDL2`

3. Verify installation:
   - Check that `C:\SDL2\include\SDL2\SDL.h` exists
   - Check that `C:\SDL2\lib\x64\libSDL2.a` (or `SDL2.lib` for VC) exists

### Option 2: Set Environment Variable

If you install SDL2 to a different location, set the `SDL2_PATH` environment variable:

```cmd
set SDL2_PATH=C:\Your\SDL2\Path
```

### Option 3: Manual Download

1. Visit: https://www.libsdl.org/download-2.0.php
2. Download "SDL2-devel-2.30.0-mingw.tar.gz" (for MinGW) or "SDL2-devel-2.30.0-VC.zip" (for Visual C++)
3. Extract to `C:\SDL2`

## Directory Structure

After installation, you should have:

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

After installation, the build system will automatically detect SDL2 at `C:\SDL2`. If you see warnings about SDL2 not being found, check:

1. SDL2 is extracted to `C:\SDL2`
2. The `include\SDL2\SDL.h` file exists
3. The `lib\x64\` directory contains the library files

## Troubleshooting

- **"SDL2 not found"**: Make sure SDL2 is extracted to `C:\SDL2` or set `SDL2_PATH` environment variable
- **"Cannot find SDL2.h"**: Check that `C:\SDL2\include\SDL2\SDL.h` exists
- **Link errors**: Make sure `C:\SDL2\lib\x64\` contains `libSDL2.a` (MinGW) or `SDL2.lib` (VC++)

