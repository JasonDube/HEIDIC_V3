# Setting Up SDL2/SDL3 Directory Structure

## Understanding the MinGW Build Structure

When you download SDL2/SDL3 for MinGW, you typically get a structure like:

```
SDL2-devel-2.30.0-mingw/
  ├── x86_64-w64-mingw32/    ← 64-bit MinGW libraries
  │   ├── bin/               ← DLL files go here
  │   ├── lib/               ← Library files (.a files)
  │   └── include/           ← Header files
  ├── i686-w64-mingw32/      ← 32-bit MinGW libraries
  └── ...
```

## Correct Installation Structure

You need to **reorganize** the contents to match what the build system expects:

### For C:\SDL2 (or C:\SDL3):

```
C:\SDL2\
  ├── include\
  │   └── SDL2\              ← Headers from x86_64-w64-mingw32/include/SDL2/
  │       ├── SDL.h
  │       └── ...
  ├── lib\
  │   ├── x64\               ← Libraries from x86_64-w64-mingw32/lib/
  │   │   ├── libSDL2.a
  │   │   └── libSDL2main.a
  │   └── x86\               ← Libraries from i686-w64-mingw32/lib/ (if needed)
  └── bin\
      └── x64\               ← DLL from x86_64-w64-mingw32/bin/
          └── SDL2.dll
```

## Quick Setup Steps

1. **Create the structure:**
   ```cmd
   mkdir C:\SDL2
   mkdir C:\SDL2\include
   mkdir C:\SDL2\lib
   mkdir C:\SDL2\lib\x64
   mkdir C:\SDL2\bin
   mkdir C:\SDL2\bin\x64
   ```

2. **Copy headers:**
   ```cmd
   xcopy /E /I "x86_64-w64-mingw32\include\SDL2" "C:\SDL2\include\SDL2"
   ```

3. **Copy libraries:**
   ```cmd
   copy "x86_64-w64-mingw32\lib\*.a" "C:\SDL2\lib\x64\"
   ```

4. **Copy DLL:**
   ```cmd
   copy "x86_64-w64-mingw32\bin\SDL2.dll" "C:\SDL2\bin\x64\"
   ```

## Alternative: Use the MinGW Structure Directly

If you prefer, you can also set `SDL2_PATH` to point directly to the `x86_64-w64-mingw32` folder, but you'll need to adjust the build system to look for:
- Headers at: `SDL2_PATH/include/SDL2/`
- Libraries at: `SDL2_PATH/lib/`
- DLL at: `SDL2_PATH/bin/`

The build system currently expects the reorganized structure above.

