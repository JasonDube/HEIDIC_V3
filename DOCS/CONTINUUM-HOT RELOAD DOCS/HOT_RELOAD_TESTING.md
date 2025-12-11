# Hot-Reload Testing Guide

## Feature 1: Automatic File Watching ✅

**Status:** Implemented

**How it works:**
- When you save a `.hd` file in H_SCRIBE, it automatically triggers hotload if the file has `@hot` systems
- No need to click the hotload button anymore!
- The editor watches for file changes and automatically rebuilds DLLs

**Testing:**
1. Create a new project or load an existing one with `@hot` systems
2. Run the project (green arrow button)
3. Edit a value in the `@hot` system (e.g., change `rotation_speed` from `1.0` to `5.0`)
4. Save the file (Ctrl+S)
5. Wait 1-2 seconds - the game should automatically reload without clicking anything!

---

## Feature 2: Shader Hot-Reload (To Be Implemented)

**Status:** 🔴 Not Started

**How it should work:**
```heidic
@hot
shader vertex "shaders/triangle.vert" {
    // Shader code
}

@hot
shader fragment "shaders/triangle.frag" {
    // Shader code
}
```

**Testing Strategy:**
1. Create a simple shader file (e.g., `test.vert.glsl`)
2. Mark it with `@hot` in the HEIDIC source
3. Run the game
4. Edit the shader file (change colors, add effects)
5. Save - shader should automatically recompile and reload
6. Visual changes should appear immediately

**Test Case:**
- Create a triangle shader that renders in red
- Run the game - triangle should be red
- Edit shader to render in blue
- Save - triangle should turn blue without restarting

---

## Feature 3: Component Hot-Reload (To Be Implemented)

**Status:** 🔴 Not Started

**How it should work:**
```heidic
@hot
component Transform {
    position: Vec3,
    rotation: Quat,
    scale: Vec3 = Vec3(1, 1, 1)
}

// Later, you add a new field:
@hot
component Transform {
    position: Vec3,
    rotation: Quat,
    scale: Vec3 = Vec3(1, 1, 1),
    velocity: Vec3 = Vec3(0, 0, 0)  // New field!
}
```

**Testing Strategy:**
1. Create entities with a component (e.g., `Transform`)
2. Run the game - entities should work normally
3. Add a new field to the component definition
4. Save - component should migrate existing entities
5. Entities should keep their old data, new field gets default value

**Test Case:**
- Create 10 entities with `Transform { position: Vec3, rotation: Quat }`
- Run the game - entities render at their positions
- Edit component to add `scale: Vec3` field
- Save - entities should migrate automatically
- All entities should keep their positions/rotations
- All entities should get default scale `Vec3(1, 1, 1)`

**Migration Testing:**
- Test adding fields (easy - just set default)
- Test removing fields (harder - need to preserve other data)
- Test changing field types (very hard - may need conversion)

---

## Implementation Notes

### For Shader Hot-Reload (#2):
- Need to watch `.glsl` or `.vert`/`.frag` files
- Recompile to SPIR-V when changed
- Rebuild Vulkan pipeline with new shaders
- This is actually relatively straightforward - shaders are already file-based

### For Component Hot-Reload (#3):
- Need to track component metadata (field names, types, offsets)
- Generate migration code that preserves existing entity data
- Handle type changes carefully (may require manual migration)
- This is the hardest feature - requires sophisticated runtime support

