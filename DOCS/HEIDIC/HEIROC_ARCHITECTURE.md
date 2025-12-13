# HEIROC: A Scripting Language Built on HEIDIC

## Overview

**HEIROC** (HEIDIC Engine Interface for Rapid Object Configuration) is a minimal scripting language that transpiles to HEIDIC. It's designed for simple configuration and asset scripting, while HEIDIC remains the full-featured language for engine and game logic.

## Philosophy

- **HEIDIC**: Full programming language for engines and game logic
- **HEIROC**: Minimal configuration/scripting DSL that transpiles to HEIDIC
- **Assets**: Packed with HEIROC scripts (like HDM files with embedded scripts)
- **Level Editor**: Assigns HEIROC scripts to assets in the scene

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    HEIROC Script                            │
│  (Simple configuration, no includes, assumes 3D context)   │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       │ Transpiles to
                       ▼
┌─────────────────────────────────────────────────────────────┐
│                    HEIDIC Code                              │
│  (Full language with includes, systems, ECS, etc.)         │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       │ Compiles to
                       ▼
┌─────────────────────────────────────────────────────────────┐
│                    C++ Code                                 │
│  (Native performance, Vulkan, GLFW, etc.)                  │
└─────────────────────────────────────────────────────────────┘
```

## HEIROC Syntax (Proposed)

### Example: Main Loop Configuration

```heiroc
// Global UI panel definition (NEUROSHELL)
PANEL* health_pan = {
    bmap = "health.dds";
    pos_x = -12;
    pos_y = 0;
    layer = 25;
    flags = VISIBLE;
}

// Main loop configuration
main_loop(
    video_resolution = 1;  // 0=640x480, 1=800x600, 2=1280x720, 3=1920x1080
    video_mode = 1;        // 0=Windowed, 1=Fullscreen, 2=Borderless
    fps_max = 75;
    random_seed = 0;
    load_level = 'planet.eden';
)

// Global level switching logic
if (level_number == 1) {
    load_level = 'planet.eden';
}
if (level_number == 2) {
    load_level = 'trader.eden';
}
if (level_number == 3) {
    load_level = 'building_2.eden';
}
```

### Example: Entity Script (packed in HDM file)

```heiroc
// Script attached to player entity
while (player) {
    health_pan.scale_x = maxv(0.01, player.health / 100);
    wait(1);  // Wait 1 frame
}
```

### Transpiles to HEIDIC:

```heidic
// Auto-generated includes (HEIDIC decides what's needed)
// Assumes 3D window context

// Global UI panel (NEUROSHELL)
let health_pan: i32 = neuroshell_create_panel(-12, 0, 100, 20);
neuroshell_set_depth(health_pan, 25.0);
neuroshell_set_visible(health_pan, true);
// TODO: Set texture "health.dds"

fn main(): void {
    // Initialize random seed
    srand(0);
    
    // Create window based on video_resolution and video_mode
    let window = heidic_create_window(800, 600, true);  // video_resolution=1, video_mode=1
    heidic_init_renderer(window);
    
    // Initialize NEUROSHELL
    neuroshell_init(window);
    
    // Level switching logic
    let level_number = get_level_number();  // From game state
    let level_to_load = "";
    if (level_number == 1) {
        level_to_load = "planet.eden";
    } else if (level_number == 2) {
        level_to_load = "trader.eden";
    } else if (level_number == 3) {
        level_to_load = "building_2.eden";
    }
    
    // Load level
    heidic_load_level(level_to_load);
    
    // Main loop with FPS cap
    let target_frame_time = 1.0 / 75.0;  // fps_max = 75
    while (!heidic_window_should_close(window)) {
        let frame_start = get_time();
        
        heidic_update();
        neuroshell_update(get_delta_time());
        heidic_render(window);
        neuroshell_render(get_command_buffer());
        
        // FPS cap
        let frame_time = get_time() - frame_start;
        if (frame_time < target_frame_time) {
            sleep((target_frame_time - frame_time) * 1000);
        }
    }
    
    neuroshell_shutdown();
    heidic_cleanup();
}
```

### Entity Script Transpiles to HEIDIC System:

```heidic
@hot
system(update_player_health_ui) {
    fn update(q: query<Player, Health>): void {
        for entity in q {
            let health_ratio = max(0.01, entity.health / 100.0);
            neuroshell_set_size(health_pan, health_ratio * 100.0, 20.0);
        }
    }
}
```

## HEIDIC DSL/Macro System

To enable HEIROC, HEIDIC needs DSL building capabilities. There are three approaches:

### Macro System Options Explained

**1. Template-Based Code Generation** (Recommended)
- HEIDIC defines templates that generate code
- Like C++ templates but for code generation
- Example: `template heiroc_main_loop { ... }` generates HEIDIC code from HEIROC parameters
- **Pros**: Type-safe, compile-time checked, integrates with HEIDIC type system
- **Cons**: More complex to implement

**2. AST Transformation**
- HEIDIC defines functions that transform AST nodes
- HEIROC parser builds AST, transformation functions convert to HEIDIC AST
- **Pros**: Powerful, can do complex transformations
- **Cons**: Requires full AST manipulation system

**3. String-Based Templates**
- HEIDIC defines string templates with placeholders
- Simple substitution: `"fn main(): void { {INIT_CODE} }"`
- **Pros**: Simple, easy to understand
- **Cons**: No type checking, error-prone

**Recommendation**: Start with **String-Based Templates** for simplicity, evolve to **Template-Based** later.

### Proposed Implementation

```heidic
// Define HEIROC → HEIDIC transformation rules
macro heiroc_main_loop(params: Map<string, any>): string {
    // Generate HEIDIC code from HEIROC parameters
    let resolution_map = [640, 480, 800, 600, 1280, 720, 1920, 1080];
    let mode_map = ["windowed", "fullscreen", "borderless"];
    
    // Extract parameters
    let res = params["video_resolution"];
    let mode = params["video_mode"];
    let level = params["load_level"];
    
    // Generate HEIDIC code
    return generate_main_loop_code(res, mode, level);
}
```

## Asset Script Packing

### HDM Files with Embedded HEIROC

```
┌─────────────────────────────────────┐
│         HDM File Header             │
│  (Geometry, texture, properties)    │
├─────────────────────────────────────┤
│      Embedded HEIROC Script         │
│  (Asset-specific behavior)          │
│  - Entity scripts (while loops)     │
│  - Event handlers                   │
│  - Component updates                │
└─────────────────────────────────────┘
```

### Script Storage Flow

1. **HDM Files**: Each HDM contains its own HEIROC script
2. **Level Files (.eden)**: Collects all HDM scripts when level loads
3. **Runtime**: All scripts transpiled together, hot-reloaded as needed

### Example: HDM with Entity Script

```heiroc
// Packed in HDM file (e.g., "player.hdm")
// This script runs on the player entity

while (player) {
    // Update health bar
    health_pan.scale_x = maxv(0.01, player.health / 100);
    
    // Handle input
    if (input_key_pressed('W')) {
        player.position += player.forward * player.speed * delta_time;
    }
    
    wait(1);  // Wait 1 frame
}
```

### Transpiles to HEIDIC System:

```heidic
@hot
system(update_player) {
    fn update(q: query<Player, Position, Health, Velocity>, delta_time: f32): void {
        for entity in q {
            // Update health bar
            let health_ratio = max(0.01, entity.health / 100.0);
            neuroshell_set_size(health_pan, health_ratio * 100.0, 20.0);
            
            // Handle input
            if (heidic_key_pressed('W')) {
                entity.position += entity.forward * entity.speed * delta_time;
            }
        }
    }
}
```

## Level Editor Integration

### Script Assignment

1. **Level Editor** (built in HEIDIC) allows:
   - Placing assets in scene
   - Assigning HEIROC scripts to assets
   - Editing script parameters

2. **Script Storage**:
   - Scripts stored in level file (`.eden` format)
   - Or embedded in HDM files
   - Or separate `.heiroc` files referenced by level

3. **Runtime**:
   - Level loads → HEIROC scripts transpile to HEIDIC
   - HEIROC scripts compile to C++ alongside engine code
   - Hot-reload support for HEIROC scripts

## Implementation Plan

### Phase 1: HEIDIC Macro System

1. **Add macro definition syntax to HEIDIC**:
   ```heidic
   macro heiroc_main_loop(params: Map<string, any>): string {
       // Generate HEIDIC code from HEIROC parameters
       return generated_heidic_code;
   }
   ```

2. **Add syntax parser hooks**:
   - Allow HEIDIC to define custom parsers
   - Map custom syntax to HEIDIC AST

3. **Add code generation templates**:
   - Template system for transforming ASTs
   - Context-aware code generation

### Phase 2: HEIROC Transpiler

1. **HEIROC Parser** (written in Rust, similar to HEIDIC compiler):
   - Parse HEIROC syntax (PANEL*, main_loop(), while loops, etc.)
   - Build HEIROC AST
   - Map to HEIDIC AST

2. **HEIROC → HEIDIC Transpiler**:
   - Transform HEIROC AST to HEIDIC AST
   - Inject includes, context, boilerplate
   - Generate full HEIDIC code
   - Handle global variables (PANEL*, etc.)
   - Convert entity scripts to @hot systems

3. **Runtime Transpilation** (Hot-Reload):
   - HEIROC scripts loaded from HDM files at runtime
   - Transpiled to HEIDIC on-the-fly
   - Compiled to C++ and hot-reloaded via @hot system
   - **Performance consideration**: Transpilation is fast (AST transformation), compilation is the bottleneck
   - **Optimization**: Cache compiled scripts, only recompile on change

### Phase 3: Asset Script Packing

1. **HDM Format Extension**:
   - Add script section to HDM files
   - Store HEIROC scripts in binary/ASCII format

2. **Script Loading**:
   - Extract scripts from HDM at runtime
   - Transpile to HEIDIC
   - Compile and hot-reload

### Phase 4: EDEN Level Editor

1. **EDEN Editor** (Separate tool, built in HEIDIC):
   - Scene graph view
   - Asset placement (HDM files)
   - Click HDM to view/edit attached HEIROC script
   - Level settings (sky, global properties, etc.)

2. **ESE-EDEN Bridge**:
   - Live connection between ESE and EDEN
   - Edit HEIROC script in ESE → auto-updates in EDEN
   - Two-way sync: Changes in EDEN reflect in ESE
   - **Implementation**: File watching + IPC or shared file system

3. **Level File Format** (`.eden`):
   - Scene graph data
   - HDM asset references (scripts embedded in HDM)
   - Global level settings (sky, lighting, etc.)
   - Level metadata

4. **Script Editing Workflow**:
   - **ESE**: Primary editor for HEIROC scripts
     - Syntax highlighting
     - Basic error checking (first compiler pass)
     - Save script → embedded in HDM
   - **EDEN**: Level editor
     - View scripts attached to HDM files
     - Trigger script editing in ESE
     - Live preview of script changes
   - **Runtime**: Full compilation
     - Deeper error checking
     - Type checking
     - Hot-reload on changes

## Benefits

1. **Simplicity**: HEIROC is minimal - no includes, assumes 3D context
2. **Power**: Transpiles to full HEIDIC, which compiles to C++
3. **Flexibility**: HEIDIC provides DSL building tools
4. **Performance**: Still compiles to native C++ (no interpreter overhead)
5. **Hot Reload**: HEIROC scripts can hot-reload via HEIDIC's @hot system
6. **Asset Scripting**: Scripts live with assets (HDM files), self-contained
7. **Level Integration**: Scripts collected from HDM files in level, transpiled together

## Performance Considerations (Hot-Reload at Runtime)

### Transpilation Performance
- **HEIROC → HEIDIC**: Fast (AST transformation, ~1-10ms for typical script)
- **HEIDIC → C++**: Fast (code generation, ~10-50ms)
- **C++ → Object Code**: Slower (compilation, ~100-500ms for small scripts)
- **Hot-Reload**: Fast (DLL loading, ~10-50ms)

### Optimization Strategies

1. **Incremental Compilation**: Only recompile changed scripts
2. **Caching**: Cache compiled scripts, invalidate on file change
3. **Background Compilation**: Compile in background thread while game runs
4. **Lazy Loading**: Only transpile/compile scripts when level loads
5. **Batch Compilation**: Compile all scripts together (better optimization)

### Expected Performance Impact

- **Initial Load**: ~500ms-2s for level with 10-20 scripts (one-time cost)
- **Hot-Reload**: ~100-500ms per script change (acceptable for iteration)
- **Runtime**: Zero overhead (scripts are compiled C++ code)

**Conclusion**: Hot-reload performance is acceptable for development iteration. For production, scripts can be pre-compiled at build time.

## Example: Complete HEIROC Game

### main.heiroc (Global Configuration)

```heiroc
// Global UI panel
PANEL* health_pan = {
    bmap = "health.dds";
    pos_x = -12;
    pos_y = 0;
    layer = 25;
    flags = VISIBLE;
}

// Main loop configuration
main_loop(
    video_resolution = 2;  // 1280x720
    video_mode = 0;        // Windowed
    fps_max = 75;
    random_seed = 0;
    load_level = 'space_station.eden';
)

// Global level switching
if (level_number == 1) {
    load_level = 'space_station.eden';
}
if (level_number == 2) {
    load_level = 'trader.eden';
}
```

### player.hdm (Entity Script)

```heiroc
// Script attached to player entity
while (player) {
    // Update health bar
    health_pan.scale_x = maxv(0.01, player.health / 100);
    
    // Movement
    if (input_key_pressed('W')) {
        player.position += player.forward * player.speed * delta_time;
    }
    
    wait(1);
}
```

### enemy.hdm (Entity Script)

```heiroc
// Script attached to enemy entity
while (enemy) {
    // AI: Move toward player
    let direction = normalize(player.position - enemy.position);
    enemy.position += direction * enemy.speed * delta_time;
    
    // Collision with player
    if (distance(enemy.position, player.position) < 1.0) {
        player.health -= 10;
    }
    
    wait(1);
}
```

This transpiles to a full HEIDIC game with ECS, systems, components, NEUROSHELL UI, etc.

## Next Steps

1. **Design HEIDIC macro/DSL system** - Start with string-based templates, evolve to template-based
2. **Design HEIROC grammar** - Finalize syntax (PANEL*, main_loop(), while loops, etc.)
3. **Prototype transpiler** - Build HEIROC → HEIDIC transpiler (Rust, similar to HEIDIC compiler)
4. **Integrate with HDM** - Add script section to HDM format (binary + ASCII)
5. **Build EDEN editor** - Separate level editor tool (built in HEIDIC)
6. **ESE-EDEN bridge** - Live connection for script editing workflow
7. **Runtime hot-reload** - Implement on-the-fly transpilation and hot-reload system

## Implementation Priority

1. **Phase 1**: HEIROC grammar and parser (Rust)
2. **Phase 2**: HEIROC → HEIDIC transpiler
3. **Phase 3**: HDM script embedding
4. **Phase 4**: Runtime hot-reload system
5. **Phase 5**: EDEN editor (basic version)
6. **Phase 6**: ESE-EDEN bridge

