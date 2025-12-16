# Hot-Reload Test Case: "Bouncing Balls"

## Overview

A comprehensive test case that exercises **all three types of hot-reload** (System, Shader, Component) while remaining simple to implement. This will help us complete and verify the component hot-reload system.

## Concept

**Multiple colored spheres bouncing around in a 3D space** with configurable movement patterns. We can hot-reload:
- **Systems**: Change movement logic (random, sine wave, orbit, etc.)
- **Shaders**: Change colors, lighting, effects
- **Components**: Add/remove fields (size, color, bounce_factor, etc.)

---

## Why This Test Case is Perfect

✅ **Simple Geometry**: Spheres can be rendered as cubes (or we can use simple sphere meshes)  
✅ **Clear Components**: Position, Velocity (obvious and intuitive)  
✅ **Visible Changes**: Movement and colors are immediately obvious  
✅ **Multiple Entities**: Tests ECS query iteration (we'll need this anyway)  
✅ **Hot-Reloadable**: Each aspect can be changed at runtime  
✅ **No Complex Features**: Doesn't need cameras, lights, collision, or textures  

---

## Implementation Plan

### Phase 1: Basic Setup (Foundation)

**Components:**
```heidic
@hot
component Position {
    x: f32,
    y: f32,
    z: f32
}

@hot
component Velocity {
    x: f32,
    y: f32,
    z: f32
}
```

**System:**
```heidic
@hot
system(movement) {
    fn update(q: query<Position, Velocity>): void {
        // Update positions based on velocity
        // Simple: position += velocity * deltaTime
        // Can hot-reload to change movement pattern
    }
}
```

**Shader:**
```heidic
@hot
shader vertex "shaders/ball.vert" {
    // Basic vertex shader with position/color
}

@hot
shader fragment "shaders/ball.frag" {
    // Fragment shader - can hot-reload to change colors/effects
}
```

**Main Loop:**
```heidic
fn main(): void {
    // Create multiple ball entities with Position + Velocity
    // Render loop that calls movement system and renders balls
}
```

### Phase 2: System Hot-Reload Testing

**Test Scenarios:**
1. **Random Movement**: Balls move randomly
2. **Sine Wave**: Balls oscillate in sine patterns
3. **Orbit**: Balls orbit around center
4. **Bounce**: Balls bounce off walls (simple boundary check)

**How to Test:**
- Start with simple linear movement
- Edit movement system → Save → Hot-reload
- Change to random movement → Hot-reload
- Change to orbit pattern → Hot-reload
- Verify changes take effect instantly

### Phase 3: Shader Hot-Reload Testing

**Test Scenarios:**
1. **Color Changes**: Change ball colors (red → blue → rainbow)
2. **Lighting Effects**: Add per-pixel lighting
3. **Pulsing**: Add time-based pulsing effect
4. **Gradient**: Add gradient effects

**How to Test:**
- Start with solid color shader
- Edit fragment shader → Compile → Hot-reload
- Change color values → Hot-reload
- Add effects → Hot-reload
- Verify visual changes appear instantly

### Phase 4: Component Hot-Reload Testing ⭐ **THE GOAL**

**Initial Component:**
```heidic
@hot
component Position {
    x: f32,
    y: f32,
    z: f32
}
```

**Test Scenario 1: Add Field**
```heidic
@hot
component Position {
    x: f32,
    y: f32,
    z: f32,
    size: f32 = 1.0  // NEW FIELD - test migration
}
```

**Test Scenario 2: Remove Field**
```heidic
@hot
component Position {
    x: f32,
    z: f32  // Removed y field - test migration
}
```

**Test Scenario 3: Change Field Type**
```heidic
@hot
component Position {
    pos: Vec3,  // Changed from x,y,z to Vec3 - test migration
    size: f32
}
```

**How to Test:**
- Start game with initial component layout
- Create entities with Position components
- Modify component definition (add/remove field)
- Recompile → Game detects layout change
- Migration function runs → Entity data migrated
- Verify entities still work with new layout

---

## Rendering Strategy

### Option 1: Render Spheres as Cubes (Simplest)
- Use existing cube rendering code
- Each "ball" is a small cube
- Simple to implement, clear visual

### Option 2: Generate Sphere Meshes
- Generate sphere vertices programmatically
- More visually accurate
- Slightly more complex

### Option 3: Instanced Rendering (Advanced)
- Render many cubes/spheres efficiently
- Good for many entities
- Best performance

**Recommendation**: Start with **Option 1** (cubes), can upgrade later.

---

## Entity Storage Requirements

For component hot-reload to work, we need:

1. **Entity Storage System**
   - Store entities with their components
   - Access components by entity ID
   - Iterate through entities with specific components (queries)

2. **Component Storage**
   - AoS (Array of Structures) or SOA (Structure of Arrays)
   - Ability to access `entity.Position.x` etc.
   - Migration functions can iterate and update

3. **Query System** (Already exists!)
   - `query<Position, Velocity>` iteration
   - Used in movement system

---

## File Structure

```
H_SCRIBE/PROJECTS/bouncing_balls/
├── bouncing_balls.hd          # Main HEIDIC source
├── bouncing_balls.cpp          # Generated C++ code
├── game_state_hot.dll.cpp      # Generated hot-reloadable DLL
├── game_state.dll              # Compiled DLL
├── shaders/
│   ├── ball.vert              # Vertex shader (hot-reloadable)
│   ├── ball.frag              # Fragment shader (hot-reloadable)
│   ├── ball.vert.spv          # Compiled SPIR-V
│   └── ball.frag.spv          # Compiled SPIR-V
└── .heidic_component_versions.txt  # Component metadata (auto-generated)
```

---

## Testing Checklist

### System Hot-Reload ✅
- [ ] Can edit movement system and hot-reload
- [ ] Movement pattern changes instantly
- [ ] No crashes or memory leaks
- [ ] Multiple hot-reloads work correctly

### Shader Hot-Reload ✅
- [ ] Can edit shaders and hot-reload
- [ ] Visual changes appear instantly
- [ ] Pipeline rebuilds correctly
- [ ] No rendering artifacts

### Component Hot-Reload ⭐
- [ ] Can add field to component
- [ ] Existing entity data migrates correctly
- [ ] New field has correct default value
- [ ] Can remove field from component
- [ ] Removed field data is handled gracefully
- [ ] Can change field type (if supported)
- [ ] Migration logs correctly
- [ ] Multiple migrations work (v1→v2→v3)

---

## Example Code Skeleton

```heidic
// bouncing_balls.hd
extern fn heidic_glfw_vulkan_hints(): void;
extern fn heidic_init_renderer_balls(window: GLFWwindow): i32;
extern fn heidic_render_balls(window: GLFWwindow, entities: [BallEntity]): void;
extern fn heidic_cleanup_renderer(): void;
extern fn heidic_sleep_ms(milliseconds: i32): void;

// Hot-reloadable components
@hot
component Position {
    x: f32,
    y: f32,
    z: f32
}

@hot
component Velocity {
    x: f32,
    y: f32,
    z: f32
}

// Hot-reloadable system
@hot
system(movement) {
    fn update(q: query<Position, Velocity>, dt: f32): void {
        for entity in q {
            // Simple movement: position += velocity * dt
            entity.Position.x += entity.Velocity.x * dt;
            entity.Position.y += entity.Velocity.y * dt;
            entity.Position.z += entity.Velocity.z * dt;
            
            // Boundary check (bounce)
            if entity.Position.x > 5.0 || entity.Position.x < -5.0 {
                entity.Velocity.x *= -1.0;
            }
            if entity.Position.y > 5.0 || entity.Position.y < -5.0 {
                entity.Velocity.y *= -1.0;
            }
            if entity.Position.z > 5.0 || entity.Position.z < -5.0 {
                entity.Velocity.z *= -1.0;
            }
        }
    }
}

// Hot-reloadable shaders
@hot
shader vertex "shaders/ball.vert" {}

@hot
shader fragment "shaders/ball.frag" {}

fn create_ball(x: f32, y: f32, z: f32, vx: f32, vy: f32, vz: f32): Entity {
    let entity = create_entity();
    add_component(entity, Position { x: x, y: y, z: z });
    add_component(entity, Velocity { x: vx, y: vy, z: vz });
    return entity;
}

fn main(): void {
    // Initialize GLFW/Vulkan...
    
    // Create initial balls
    let balls: [Entity] = [];
    balls.push(create_ball(0.0, 0.0, 0.0, 1.0, 0.5, 0.3));
    balls.push(create_ball(1.0, 1.0, 1.0, -0.5, 0.8, -0.2));
    balls.push(create_ball(-1.0, -1.0, -1.0, 0.3, -0.7, 0.4));
    // ... more balls
    
    let last_time = get_time();
    
    while glfwWindowShouldClose(window) == 0 {
        glfwPollEvents();
        
        // Calculate delta time
        let current_time = get_time();
        let dt = current_time - last_time;
        last_time = current_time;
        
        // Hot-reloadable movement update
        if g_movement_update != nullptr {
            g_movement_update(q_all_balls, dt);
        }
        
        // Render all balls
        heidic_render_balls(window, balls);
        
        heidic_sleep_ms(16);
    }
}
```

---

## Next Steps

1. **Implement Basic Rendering**
   - Add `heidic_render_balls()` function to Vulkan helpers
   - Render multiple cubes/spheres at different positions

2. **Implement Entity Storage** (Required for component hot-reload)
   - Simple entity storage system
   - Component storage (AoS or SOA)
   - Query iteration support

3. **Create Test Project**
   - Set up `bouncing_balls.hd` project
   - Basic movement system
   - Basic shaders

4. **Test System Hot-Reload**
   - Verify movement changes work

5. **Test Shader Hot-Reload**
   - Verify visual changes work

6. **Complete Component Hot-Reload** ⭐
   - Implement entity storage integration
   - Test component migration
   - Verify entity data persists across layout changes

---

## Benefits

1. **Tests All Hot-Reload Types**: System, Shader, Component
2. **Clear Visual Feedback**: Easy to see if hot-reload worked
3. **Simple Implementation**: No complex features needed
4. **Scalable**: Can add more balls, more components, more systems
5. **Real-World Scenario**: Similar to actual game development workflow

---

## Success Criteria

✅ Can hot-reload movement system and see pattern change instantly  
✅ Can hot-reload shaders and see visual changes instantly  
✅ Can add field to component and existing entities migrate correctly  
✅ Can remove field from component and migration handles it  
✅ Multiple hot-reloads work without crashes  
✅ Game runs smoothly with 10+ balls  

---

*This test case will help us complete the component hot-reload system and prove the entire hot-reload infrastructure works correctly!*

