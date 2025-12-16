# CONTINUUM: Component Hot-Loading Explained

**Part of EDEN Engine's CONTINUUM hot-reload system**

## What is Component Hot-Loading?

**Component hot-loading** allows you to change the structure of a component (add/remove fields, change types) **while your game is running**, and automatically migrate existing entity data to the new layout.

## The Problem It Solves

In a traditional game engine, if you change a component definition:
```heidic
// Old version
component Transform {
    position: Vec3,
    rotation: Vec3  // Euler angles
}

// New version - you added a field and changed a type
component Transform {
    position: Vec3,
    rotation: Quat,  // Changed from Vec3 to Quat
    scale: Vec3      // New field!
}
```

**Without hot-loading:**
- ❌ You must restart the game
- ❌ All entities lose their data
- ❌ You lose your game state

**With hot-loading:**
- ✅ Game keeps running
- ✅ Entities keep their existing data
- ✅ Data is automatically migrated to new layout

## How It Works

### 1. Mark Component as `@hot`
```heidic
@hot
component Transform {
    position: Vec3,
    rotation: Vec3,
}
```

### 2. Change Component Structure
While the game is running, you edit the component:
```heidic
@hot
component Transform {
    position: Vec3,
    rotation: Quat,  // Changed type!
    scale: Vec3      // Added field!
}
```

### 3. Automatic Migration
When you hot-reload, HEIDIC:
1. **Detects the change** (component layout changed)
2. **Maps old fields to new fields:**
   - `position: Vec3` → `position: Vec3` (unchanged, copy data)
   - `rotation: Vec3` → `rotation: Quat` (changed type, convert data)
   - (nothing) → `scale: Vec3` (new field, use default value)
3. **Updates all entities** that have this component
4. **Game continues** with new component layout

## Real-World Example

### Before Hot-Load:
```heidic
@hot
component Health {
    current: f32,
    max: f32,
}
```

You have 100 entities with `Health` components, all with different health values.

### You Change It:
```heidic
@hot
component Health {
    current: f32,
    max: f32,
    regeneration: f32,  // New field!
}
```

### After Hot-Load:
- All 100 entities keep their `current` and `max` values
- All 100 entities get `regeneration: 0.0` (default value for new field)
- **Game never stopped!**

## Migration Rules

### Automatic Migrations:
1. **Unchanged fields** → Copy data directly
2. **New fields** → Use default values (0, empty, etc.)
3. **Removed fields** → Discard data (can't recover)

### Type Conversions:
Some conversions are automatic:
- `f32` → `f64` (widening)
- `i32` → `i64` (widening)
- `Vec3` → `Quat` (needs conversion function)

Some need manual migration code:
```heidic
@hot
component Transform {
    position: Vec3,
    rotation: Vec3,  // Old
}

// You want to change to:
@hot
component Transform {
    position: Vec3,
    rotation: Quat,  // New - needs conversion!
}

// Migration function (generated or manual):
fn migrate_rotation(old: Vec3) -> Quat {
    // Convert Euler angles to quaternion
    return euler_to_quat(old);
}
```

## Implementation Complexity

This is **the hardest part** of hot-reloading because:

### System Hot-Reload:
- ✅ Just swap function pointers
- ✅ No data migration needed
- ✅ Simple!

### Shader Hot-Reload:
- ✅ Recompile shader, rebuild pipeline
- ✅ No data migration needed
- ✅ Medium complexity

### Component Hot-Reload:
- ❌ Need to migrate entity data
- ❌ Need to handle type conversions
- ❌ Need to handle SOA layout changes (even harder!)
- ❌ Need to update all queries/systems that use the component
- ✅ **Most complex!**

## How HEIDIC Will Implement It

### Step 1: Component Metadata
Generate metadata for each `@hot` component:
- Field names and types
- Field offsets
- Default values
- Size in bytes

### Step 2: Version Tracking
Track component versions:
```cpp
struct ComponentVersion {
    std::string component_name;
    uint32_t version;  // Increments when layout changes
    // Layout metadata
};
```

### Step 3: Migration Functions
Generate migration functions:
```cpp
Transform migrate_Transform_v1_to_v2(Transform_v1 old) {
    Transform_v2 new_component;
    new_component.position = old.position;  // Copy unchanged field
    new_component.rotation = vec3_to_quat(old.rotation);  // Convert
    new_component.scale = Vec3(1.0, 1.0, 1.0);  // Default for new field
    return new_component;
}
```

### Step 4: Runtime Migration
During hot-reload:
1. Check if component layout changed
2. For each entity with that component:
   - Load old component data
   - Run migration function
   - Store new component data
3. Update component registry
4. Continue game loop

## Example Workflow

```heidic
// Initial code
@hot
component Player {
    health: f32,
    position: Vec3,
}

fn main(): void {
    // Create entity with Player component
    let player = create_entity();
    add_component(player, Player { health: 100.0, position: Vec3(0, 0, 0) });
    
    // Game is running...
    // You edit the component:
}

// You change it (while game runs):
@hot
component Player {
    health: f32,
    position: Vec3,
    mana: f32,  // Added new field
}

// You save and hot-reload
// Result: Your player entity now has:
// - health: 100.0 (preserved!)
// - position: Vec3(0, 0, 0) (preserved!)
// - mana: 0.0 (new field with default value)
// Game never stopped!
```

## Why It's Powerful

This is the **holy grail** of hot-reloading because:
- ✅ Iterate on game data structures without restarting
- ✅ Test balance changes live (change `Health.max` field)
- ✅ Add features to existing entities (add `mana` field)
- ✅ Zero downtime during development

## Current Status

**Status:** 🔴 Not Yet Implemented

**What's Done:**
- ✅ `@hot` attribute parsing (for systems and shaders)
- ✅ System hot-reload (DLL swapping)
- ✅ Shader hot-reload (SPIR-V recompilation)

**What's Needed:**
- ❌ `@hot` on components (parser support)
- ❌ Component metadata generation
- ❌ Version tracking system
- ❌ Migration function generation
- ❌ Runtime data migration
- ❌ SOA component migration (even harder!)

## Comparison to Other Hot-Reload Features

| Feature | Complexity | Data Migration | Status |
|---------|-----------|----------------|--------|
| **System Hot-Reload** | ⭐ Easy | None | ✅ Done |
| **Shader Hot-Reload** | ⭐⭐ Medium | None | ✅ Done |
| **Component Hot-Reload** | ⭐⭐⭐⭐⭐ Hard | Required | ❌ Not Started |

## Next Steps

To implement component hot-loading, we need:
1. Parse `@hot` on components
2. Generate component metadata
3. Track component versions
4. Generate migration functions
5. Implement runtime migration
6. Handle SOA components (special case)

This is a **complex feature** that requires careful design, but it's the final piece of the hot-reload puzzle!

