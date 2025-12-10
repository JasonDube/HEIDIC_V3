# Hot-Reload Implementation Status

## ✅ Completed

### 1. @hot Attribute Parsing
- ✅ Added `@hot` token to lexer
- ✅ Added `is_hot` field to `SystemDef` in AST
- ✅ Parser recognizes `@hot system(...)` syntax
- ✅ Test file compiles successfully

**Example:**
```heidic
@hot
system(rotation) {
    fn get_rotation_speed(): f32 {
        let rotation_speed: f32 = 1.0;
        return rotation_speed;
    }
}
```

### 2. Hot-Reload Infrastructure (Basic)
- ✅ Created `hot_reload.rs` module
- ✅ File watcher using `notify` crate (cross-platform)
- ✅ Windows DLL loader using `windows-sys`
- ✅ `HotReloadManager` struct for managing hot-reload state

### 3. Test Example
- ✅ Created `examples/hot_reload_test/hot_reload_test.hd`
- ✅ Demonstrates `@hot` system with rotation speed
- ✅ Ready for full hot-reload implementation

---

## 🔄 In Progress / Next Steps

### 1. Generate Hot-Reloadable Systems as DLLs
**Status:** Not started  
**What's needed:**
- Modify codegen to detect `@hot` systems
- Generate separate DLL project for each hot-reloadable system
- Export functions with C linkage (`extern "C"`)
- Generate DLL compilation commands

**Example output:**
```cpp
// rotation_system.dll.cpp
extern "C" {
    float get_rotation_speed() {
        float rotation_speed = 1.0f;
        return rotation_speed;
    }
}
```

### 2. Runtime Hot-Reload Loop
**Status:** Not started  
**What's needed:**
- Main game loop checks for file changes
- When change detected:
  1. Recompile hot-reloadable DLL
  2. Unload old DLL
  3. Load new DLL
  4. Update function pointers
- Continue game loop without interruption

**Pseudo-code:**
```rust
loop {
    // Check for file changes
    if hot_reload_manager.check_for_changes()? {
        // Recompile DLL
        recompile_hot_system()?;
        // Reload DLL
        reload_dll()?;
    }
    
    // Game loop
    game_update();
    game_render();
}
```

### 3. Function Pointer Management
**Status:** Not started  
**What's needed:**
- Store function pointers in a table
- Update pointers when DLL reloads
- Ensure type safety

**Example:**
```cpp
// Function pointer table
struct HotReloadTable {
    float (*get_rotation_speed)();
};

HotReloadTable* g_hot_reload_table = nullptr;

// Usage
float speed = g_hot_reload_table->get_rotation_speed();
```

### 4. Integration with Spinning Triangle
**Status:** Not started  
**What's needed:**
- Update spinning triangle example to use `@hot` system
- Extract rotation speed to hot-reloadable function
- Test hot-reload by changing speed while game runs

---

## 📋 Implementation Plan

### Phase 1: Codegen for Hot-Reloadable Systems (1-2 days)
1. Detect `@hot` systems in codegen
2. Generate separate C++ file for each hot system
3. Export functions with `extern "C"`
4. Generate DLL compilation script

### Phase 2: Runtime Integration (2-3 days)
1. Create hot-reload runtime in C++
2. Implement DLL loading/unloading
3. Implement function pointer table
4. Integrate with main game loop

### Phase 3: File Watching & Auto-Reload (1 day)
1. Integrate file watcher with game loop
2. Auto-detect changes and trigger reload
3. Add error handling and recovery

### Phase 4: Testing (1 day)
1. Test with spinning triangle example
2. Verify rotation speed changes work
3. Test error cases (compilation errors, etc.)

---

## 🎯 Current Test Example

**File:** `examples/hot_reload_test/hot_reload_test.hd`

**Features:**
- `@hot` system with `get_rotation_speed()` function
- Rotation speed can be changed (currently `1.0`)
- Ready for hot-reload integration

**To test (once full implementation is done):**
1. Run the game
2. Edit `rotation_speed` value in the `@hot` system
3. Save the file
4. Game automatically reloads the system
5. Triangle spins faster/slower immediately

---

## 🔧 Technical Details

### File Watcher
- Uses `notify` crate (cross-platform)
- Watches `.hd` source files
- Detects file modification events

### DLL Loading (Windows)
- Uses `windows-sys` crate
- `LoadLibraryW` to load DLL
- `GetProcAddress` to get function pointers
- `FreeLibrary` to unload DLL

### Codegen Changes Needed
- Detect `is_hot` flag on systems
- Generate separate DLL source files
- Export functions with C linkage
- Generate DLL compilation commands

---

## 📝 Notes

- Full hot-reload requires DLL compilation, which adds complexity
- For initial test, we could use a simpler approach:
  - Recompile entire game on file change
  - Restart game automatically
  - This is "warm reload" not "hot reload" but easier to implement

- True hot-reload (no restart) requires:
  - DLL compilation
  - Runtime DLL loading
  - Function pointer management
  - State preservation

---

*Last updated: After initial @hot attribute implementation*

