# HEIDIC Component Framework Guide

## Overview

This guide explains how to use the HEIDIC ECS Component Framework (`stdlib/ecs_components.hd`), which provides a modern, flexible approach to entity management based on analysis of legacy game engine systems.

---

## Philosophy: Components Over Entity Types

### Old Approach (2001-style)
```
Entity Type → Determines everything
- Level Entity: actors, vehicles, vegetation
- View Entity: 3D panel elements  
- Sky Entity: sky, background, horizon
```

**Problems:**
- Rigid: Can't change entity type
- Inconsistent: Different flags (INVISIBLE vs SHOW)
- Mixed concerns: Rendering + creation + events all tied together

### New Approach (Modern ECS)
```
Components → Flexible composition
- RenderLayer component: where to render
- Visible component: visibility
- CoordinateSystem component: coordinate space
- EventHandler component: what events to handle
```

**Benefits:**
- Flexible: Add/remove components to change behavior
- Consistent: Same component pattern everywhere
- Separated: Rendering, events, coordinates are independent
- Queryable: `query<RenderLayer, Visible>` finds all visible entities

---

## Core Components

### 1. RenderLayer Component

Determines where an entity renders.

```heidic
component RenderLayer {
    layer: RenderLayerType
}

enum RenderLayerType {
    Level,              // Main game world
    ScreenForeground,   // UI/overlay elements
    LevelBackground,    // Sky, background, horizon
    ScreenBackground    // Full-screen effects
}
```

**Usage:**
```heidic
// Create a level entity
add_component(entity, RenderLayer { layer: RenderLayerType.Level });

// Create a UI element
add_component(entity, RenderLayer { layer: RenderLayerType.ScreenForeground });

// Create a sky entity
add_component(entity, RenderLayer { layer: RenderLayerType.LevelBackground });
```

### 2. Visible Component

Consistent visibility handling (replaces INVISIBLE vs SHOW flags).

```heidic
component Visible {
    is_visible: bool = true  // Default: visible
}
```

**Usage:**
```heidic
// Make entity visible
add_component(entity, Visible { is_visible: true });

// Hide entity (equivalent to setting INVISIBLE flag)
add_component(entity, Visible { is_visible: false });

// Show entity (equivalent to setting SHOW flag)
// Just set is_visible to true
entity.Visible.is_visible = true;
```

### 3. CoordinateSystem Component

Determines which coordinate space the entity uses.

```heidic
component CoordinateSystem {
    system: CoordSystemType
}

enum CoordSystemType {
    World,      // World coordinates (level entities)
    View,       // View/camera coordinates (UI elements)
    Screen,     // Screen/pixel coordinates (2D UI)
    Mixed       // Mixed coordinates (sky entities)
}
```

**Usage:**
```heidic
// Level entity uses world coordinates
add_component(entity, CoordinateSystem { system: CoordSystemType.World });

// UI element uses view coordinates
add_component(entity, CoordinateSystem { system: CoordSystemType.View });

// Sky entity uses mixed coordinates
add_component(entity, CoordinateSystem { system: CoordSystemType.Mixed });
```

### 4. EventHandler Component

Determines what events the entity responds to.

```heidic
component EventHandler {
    handles_collision: bool = false,
    handles_mouse: bool = false,
    handles_keyboard: bool = false,
    handles_touch: bool = false
}
```

**Usage:**
```heidic
// Level entity handles collision and mouse
add_component(entity, EventHandler { 
    handles_collision: true, 
    handles_mouse: true 
});

// UI element handles mouse only
add_component(entity, EventHandler { 
    handles_mouse: true 
});

// Sky entity handles no events
add_component(entity, EventHandler { 
    handles_collision: false, 
    handles_mouse: false 
});
```

### 5. VisualOrder Component

Determines rendering order (replaces view distance vs layer ordering).

```heidic
component VisualOrder {
    order_type: OrderType,
    layer: i32 = 0,              // Layer number
    view_distance: f32 = 0.0,    // Distance from camera
    z_index: f32 = 0.0           // Z-index for 2D elements
}

enum OrderType {
    ViewDistance,   // Order by distance (level entities)
    Layer,          // Order by layer (UI elements)
    Both,           // Order by layer, then distance (sky entities)
    ZIndex          // Order by Z-index (2D elements)
}
```

**Usage:**
```heidic
// Level entity: order by view distance
add_component(entity, VisualOrder { 
    order_type: OrderType.ViewDistance 
});

// UI element: order by layer
add_component(entity, VisualOrder { 
    order_type: OrderType.Layer,
    layer: 0
});

// Sky entity: order by layer, then distance
add_component(entity, VisualOrder { 
    order_type: OrderType.Both,
    layer: 0,
    view_distance: 0.0
});
```

---

## Position Components

Different position components for different coordinate systems:

### Position (World Coordinates)
```heidic
component Position {
    x: f32,
    y: f32,
    z: f32
}
```
For level entities in world space.

### ViewPosition (View Coordinates)
```heidic
component ViewPosition {
    x: f32,
    y: f32,
    z: f32
}
```
For UI elements in view/camera space.

### ScreenPosition (Screen Coordinates)
```heidic
component ScreenPosition {
    x: f32,  // Screen X (0.0 to 1.0 or pixels)
    y: f32   // Screen Y (0.0 to 1.0 or pixels)
}
```
For 2D UI elements in screen space.

### SkyPosition (Mixed Coordinates)
```heidic
component SkyPosition {
    view_x: f32,
    view_y: f32,
    view_z: f32,
    world_rotation_x: f32,
    world_rotation_y: f32,
    world_rotation_z: f32
}
```
For sky entities with mixed coordinates.

---

## Rendering Components

### Model (3D)
```heidic
component Model {
    mesh: string,
    texture: string = "",
    material: string = ""
}
```
For 3D models (level entities, sky entities).

### Sprite (2D)
```heidic
component Sprite {
    texture: string,
    width: f32 = 1.0,
    height: f32 = 1.0,
    uv_x: f32 = 0.0,
    uv_y: f32 = 0.0,
    uv_width: f32 = 1.0,
    uv_height: f32 = 1.0
}
```
For 2D sprites (UI elements, sprites).

### Terrain
```heidic
component Terrain {
    heightmap: string,
    size_x: f32 = 100.0,
    size_z: f32 = 100.0,
    height_scale: f32 = 1.0
}
```
For terrain (level entities).

---

## Complete Examples

### Example 1: Create a Level Entity (Actor/Vehicle/Vegetation)

```heidic
fn create_level_entity(): void {
    let entity = create_entity();
    
    // Position in world coordinates
    add_component(entity, Position { x: 0, y: 0, z: 0 });
    
    // 3D model
    add_component(entity, Model { mesh: "actor.obj" });
    
    // Render in level layer
    add_component(entity, RenderLayer { layer: RenderLayerType.Level });
    
    // Visible by default
    add_component(entity, Visible { is_visible: true });
    
    // Handle collision and mouse events
    add_component(entity, EventHandler { 
        handles_collision: true, 
        handles_mouse: true 
    });
    
    // Use world coordinates
    add_component(entity, CoordinateSystem { system: CoordSystemType.World });
    
    // Order by view distance
    add_component(entity, VisualOrder { 
        order_type: OrderType.ViewDistance 
    });
}
```

### Example 2: Create a View Entity (3D Panel Element)

```heidic
fn create_view_entity(): void {
    let entity = create_entity();
    
    // Position in view coordinates
    add_component(entity, ViewPosition { x: 0, y: 0, z: 0 });
    
    // Sprite texture
    add_component(entity, Sprite { texture: "panel.png" });
    
    // Render in screen foreground
    add_component(entity, RenderLayer { layer: RenderLayerType.ScreenForeground });
    
    // Hidden by default (set SHOW flag = set is_visible to true)
    add_component(entity, Visible { is_visible: false });
    
    // No event handling
    add_component(entity, EventHandler { 
        handles_collision: false, 
        handles_mouse: false 
    });
    
    // Use view coordinates
    add_component(entity, CoordinateSystem { system: CoordSystemType.View });
    
    // Order by layer
    add_component(entity, VisualOrder { 
        order_type: OrderType.Layer,
        layer: 0
    });
}
```

### Example 3: Create a Sky Entity (Sky/Background/Horizon)

```heidic
fn create_sky_entity(): void {
    let entity = create_entity();
    
    // Mixed coordinates (view position + world rotation)
    add_component(entity, SkyPosition { 
        view_x: 0, view_y: 0, view_z: 0,
        world_rotation_x: 0, world_rotation_y: 0, world_rotation_z: 0
    });
    
    // Sky model
    add_component(entity, Model { mesh: "sky.obj" });
    
    // Render in level background
    add_component(entity, RenderLayer { layer: RenderLayerType.LevelBackground });
    
    // Hidden by default (set SHOW flag = set is_visible to true)
    add_component(entity, Visible { is_visible: false });
    
    // No event handling
    add_component(entity, EventHandler { 
        handles_collision: false, 
        handles_mouse: false 
    });
    
    // Use mixed coordinates
    add_component(entity, CoordinateSystem { system: CoordSystemType.Mixed });
    
    // Order by layer, then view distance
    add_component(entity, VisualOrder { 
        order_type: OrderType.Both,
        layer: 0,
        view_distance: 0.0
    });
}
```

---

## Query-Based Systems

### Render Level Entities

```heidic
@system(render_level)
fn render_level_entities(q: query<Position, Model, RenderLayer, Visible>): void {
    for entity in q {
        // Only render if layer is Level and visible
        if entity.RenderLayer.layer == RenderLayerType.Level && entity.Visible.is_visible {
            draw_model(entity.Model.mesh, entity.Position);
        }
    }
}
```

### Render View Entities (UI)

```heidic
@system(render_ui, after = render_level)
fn render_view_entities(q: query<ViewPosition, Sprite, RenderLayer, Visible, VisualOrder>): void {
    for entity in q {
        if entity.RenderLayer.layer == RenderLayerType.ScreenForeground && entity.Visible.is_visible {
            // Sort by layer, then render
            draw_sprite(entity.Sprite.texture, entity.ViewPosition, entity.VisualOrder.layer);
        }
    }
}
```

### Render Sky Entities

```heidic
@system(render_sky, before = render_level)
fn render_sky_entities(q: query<SkyPosition, Model, RenderLayer, Visible, VisualOrder>): void {
    for entity in q {
        if entity.RenderLayer.layer == RenderLayerType.LevelBackground && entity.Visible.is_visible {
            // Render sky with mixed coordinates
            draw_sky_model(entity.Model.mesh, entity.SkyPosition, entity.VisualOrder);
        }
    }
}
```

---

## Benefits Summary

1. **Flexible**: Entity can change category by adding/removing components
2. **Consistent**: Same component pattern everywhere (no INVISIBLE vs SHOW flags)
3. **Queryable**: Easy to find entities with specific combinations
4. **Extensible**: Add new components without changing entity "types"
5. **Performance**: Query only processes entities with required components

---

## Migration from Legacy Systems

### Old Way (2001)
```c
// Create level entity
ENTITY* ent = ent_create(ENTITY_LEVEL, ...);
ent->flags &= ~INVISIBLE;  // Make visible

// Create view entity
ENTITY* ui = ent_create(ENTITY_VIEW, ...);
ui->flags2 |= SHOW;  // Show flag

// Create sky entity
ENTITY* sky = ent_createlayer(ENTITY_SKY, ...);
sky->flags2 |= SHOW;  // Show flag
```

### New Way (HEIDIC)
```heidic
// Create level entity
let ent = create_entity();
add_component(ent, RenderLayer { layer: RenderLayerType.Level });
add_component(ent, Visible { is_visible: true });

// Create view entity
let ui = create_entity();
add_component(ui, RenderLayer { layer: RenderLayerType.ScreenForeground });
add_component(ui, Visible { is_visible: true });  // Set SHOW flag

// Create sky entity
let sky = create_entity();
add_component(sky, RenderLayer { layer: RenderLayerType.LevelBackground });
add_component(sky, Visible { is_visible: true });  // Set SHOW flag
```

---

*This framework provides a solid foundation for entity management in HEIDIC, avoiding the pitfalls of rigid entity types and inconsistent flags.*

