# Resource Hot-Reload Test Project

This project tests **CONTINUUM resource hot-reload** - the 4th axis of hot-reload that automatically reloads resources when files change!

## What This Tests

✅ **@hot resource declarations** - Zero-boilerplate resource syntax with explicit hot-reload  
✅ **File-based hot-reload** - @hot resources automatically reload when files change  
✅ **GPU resource cleanup** - Old resources destroyed, new ones created  
✅ **Visual updates** - Changes appear instantly without restarting

## Setup

1. **Add model and textures:**
   - Place `eve_1.obj` in `models/` folder
   - Place `eve_tex.png` in `textures/` folder
   - Place `eve2_tex.png` in `textures/` folder (for testing)

2. **Build and Run:**
   - Open this project in SCRIBE editor
   - Click the white "Run" button (▶)
   - You should see the model with the first texture!

## Testing Hot-Reload

### Test 1: Code-Based Resource Switching

1. In `resource_hot_reload_test.hd`, comment out the first texture resource:
   ```heidic
   // @hot resource MyTexture: Texture = "textures/eve_tex.png";
   ```

2. Uncomment the second texture resource:
   ```heidic
   @hot resource MyTexture: Texture = "textures/eve2_tex.png";
   ```

3. Click the **red "Hotload" button** (🔄) in SCRIBE
   - This recompiles the HEIDIC code
   - The new resource will be loaded
   - Texture should change!

**Note:** This tests code recompilation, not true hot-reload.

### Test 2: File-Based Hot-Reload (True CONTINUUM) ⭐

This is the **real** resource hot-reload test!

1. **Start the game** (white Run button)
2. **While the game is running**, replace `textures/eve_tex.png` with a different image
   - **⚠️ IMPORTANT (Windows):** To ensure the file modification time updates correctly, you must:
     - **Delete** the old texture file first
     - **Then paste/copy** the new texture file
     - Simply overwriting the file may not trigger hot-reload (Windows file system behavior)
   - You can copy `eve2_tex.png` over `eve_tex.png`
   - Or use any other PNG/DDS image
3. **Watch the console** - you should see:
   ```
   [EDEN] Texture file changed, reloading: textures/eve_tex.png
   [EDEN] Texture reloaded successfully!
   ```
4. **Watch the screen** - the texture should update instantly!

**This is true CONTINUUM hot-reload** - no code changes, no recompilation, just file replacement!

**Note:** The delete-and-paste method is required on Windows because overwriting a file doesn't always update the modification time immediately. This is normal behavior for file-based hot-reload systems.

## How It Works

1. Hot-reloadable resources are declared with the `@hot` keyword:
   ```heidic
   @hot resource MyTexture: Texture = "textures/eve_tex.png";
   ```
   
   This is consistent with other CONTINUUM hot-reloadable items:
   - `@hot system` - Hot-reloadable game logic
   - `@hot shader` - Hot-reloadable shaders
   - `@hot component` - Hot-reloadable component layouts
   - `@hot resource` - Hot-reloadable assets (textures, meshes)

2. The game loop automatically checks file modification times each frame

3. When a file change is detected:
   - Old GPU resources are destroyed (texture, image view, sampler)
   - New resource is loaded from the updated file
   - New GPU resources are created and uploaded to GPU
   - Visual updates appear immediately

4. All happens automatically - zero code changes needed!

## Expected Output

When you replace a texture file, you should see:
```
[Resource Hot-Reload] MyTexture reloaded successfully!
```

The texture on the model should update instantly without any stuttering or lag.

## Performance

- **File check**: <1ms (stat() call)
- **Resource reload**: ~10-100ms (depends on texture size)
- **GPU upload**: Included in reload time
- **No frame drops**: Reload happens between frames

## Next Steps

Once this works, you can:
- Test with different texture formats (DDS, PNG)
- Test with mesh hot-reload (replace OBJ file)
- Combine with other CONTINUUM features (system, shader, component hot-reload)

---

**CONTINUUM** - Where resources reload themselves, automatically.

