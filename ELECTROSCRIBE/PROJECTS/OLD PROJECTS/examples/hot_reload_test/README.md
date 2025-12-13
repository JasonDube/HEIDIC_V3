# Hot-Reload Test Example

This example demonstrates HEIDIC's hot-reload functionality. You can edit code while the game is running and see changes instantly!

## Quick Start

### 1. Build Everything

```bash
build.bat
```

This will:
- Compile the HEIDIC source file
- Generate the hot-reloadable DLL source
- Compile the DLL
- Compile the main executable

### 2. Run the Game

```bash
run.bat
```

Or directly:
```bash
hot_reload_test.exe
```

### 3. Test Hot-Reload

**IMPORTANT**: The game must be running first! Start it with `run.bat` or `hot_reload_test.exe` in a separate window/terminal.

1. **Edit the code**: Open `hot_reload_test.hd` and change the `rotation_speed` value (line ~24)
   ```heidic
   let rotation_speed: f32 = 2.0;  // Change from 1.0 to 2.0, 5.0, etc.
   ```

2. **Save the file**

3. **Rebuild the DLL**: Run `rebuild_dll.bat` (this will recompile the HEIDIC source and rebuild the DLL):
   ```bash
   rebuild_dll.bat
   ```

4. **Watch the magic**: The game automatically detects the DLL change and reloads it. The triangle spins faster/slower immediately!

**Note**: The game does NOT start automatically after `rebuild_dll.bat`. You need to run the game separately first with `run.bat` or `hot_reload_test.exe`.

## Files

- `hot_reload_test.hd` - HEIDIC source file with `@hot` system
- `hot_reload_test.cpp` - Generated main game code
- `rotation_hot.dll.cpp` - Generated hot-reloadable DLL source
- `rotation.dll` - Compiled DLL (hot-reloadable)
- `hot_reload_test.exe` - Main executable

## Scripts

- `build.bat` - Full build (compile everything)
- `rebuild_dll.bat` - Quick rebuild of just the DLL (for hot-reload testing)
- `run.bat` - Run the game with instructions

## How It Works

1. The `@hot` system is compiled into a separate DLL
2. The main game loads the DLL and uses function pointers
3. Each frame, the game checks if the DLL source file changed
4. If changed, it automatically reloads the DLL
5. Your changes take effect instantly!

## Troubleshooting

### DLL not found
- Make sure you've run `build.bat` first
- Check that `rotation.dll` exists in the same directory as the executable

### Changes not reloading
- Make sure you rebuild the DLL after editing the `.hd` file
- The game checks for changes each frame, so it should reload automatically
- Check the console output for reload messages

### Compilation errors
- Make sure you have `g++` installed and in your PATH
- Check that all required libraries are available
- Make sure you're in the correct directory when running the scripts

## Next Steps

Try changing different values:
- `rotation_speed = 0.5` - Slow rotation
- `rotation_speed = 2.0` - Fast rotation
- `rotation_speed = 5.0` - Very fast rotation
- `rotation_speed = -1.0` - Reverse rotation

The game will reload instantly each time!

