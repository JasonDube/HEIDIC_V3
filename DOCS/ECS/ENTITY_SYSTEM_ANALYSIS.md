# Entity System Analysis - 2001 Design Review

## Original Entity Breakdown (2001)

### Three Entity Categories:

1. **Level Entities**
   - Types: Model, map, sprite, terrain
   - Used for: actors, vehicles, vegetation
   - Rendered in: level
   - Visible when: INVISIBLE flag not set
   - Events: collision & mouse
   - Coordinates: world coordinates

2. **View Entities**
   - Types: Model, sprite
   - Used for: 3D panel elements
   - Rendered in: screen foreground
   - Visible when: SHOW flag set (flags2)
   - Events: none
   - Coordinates: view coordinates

3. **Sky Entities**
   - Types: Model, sprite
   - Used for: sky, background, horizon
   - Rendered in: level background
   - Visible when: SHOW flag set (flags2)
   - Events: none
   - Coordinates: view position, world rotation

---

## Analysis: Is This a Good Breakdown?

### ✅ **What Works Well (For 2001)**

1. **Clear Separation of Concerns**
   - Level entities = game world objects
   - View entities = UI/overlay elements
   - Sky entities = background/atmosphere
   - This separation makes sense conceptually

2. **Different Rendering Contexts**
   - Level: main game world
   - Screen foreground: UI elements
   - Level background: sky/atmosphere
   - Each has different rendering requirements

3. **Different Coordinate Systems**
   - World coordinates (level entities)
   - View coordinates (view entities)
   - Mixed (sky entities)
   - This reflects real rendering needs

4. **Event Handling Separation**
   - Only level entities need collision/mouse events
   - View/sky entities are purely visual
   - Makes sense for performance

### ⚠️ **Issues with This Approach (Modern Perspective)**

1. **Entity "Types" vs Components**
   - Using entity categories/types is rigid
   - Better: Use components to mark entity category
   - More flexible: entity can change category by adding/removing components

2. **Flag-Based Visibility**
   - `INVISIBLE` flag vs `SHOW` flag is inconsistent
   - Better: `Visible` component (boolean)
   - Or: `RenderLayer` component (enum: Level, UI, Sky)

3. **Rendering Context as Entity Type**
   - Rendering context should be a component, not entity type
   - Entity could be in multiple contexts (e.g., level entity that also renders in UI)

4. **Mixed Concerns**
   - Entity creation method tied to category
   - Better: Create entities generically, add components to categorize

---

## Modern ECS Refactoring

### Component-Based Approach

Instead of entity "types", use components:

```heidic
// Render layer component - determines where entity renders
component RenderLayer {
    layer: RenderLayerType  // Level, ScreenForeground, LevelBackground
}

enum RenderLayerType {
    Level,
    ScreenForeground,
    LevelBackground
}

// Visibility component
component Visible {
    is_visible: bool
}

// Coordinate system component
component CoordinateSystem {
    system: CoordSystemType  // World, View, Mixed
}

enum CoordSystemType {
    World,
    View,
    Mixed
}

// Event handling component
component EventHandler {
    handles_collision: bool,
    handles_mouse: bool
}

// Visual ordering component
component VisualOrder {
    order_type: OrderType,  // ViewDistance, Layer, Both
    layer: i32,              // For layer-based ordering
    view_distance: f32      // For distance-based ordering
}
```

### Example: Level Entity (Modern ECS)

```heidic
// Create a level entity (actor/vehicle/vegetation)
component Position {
    x: f32,
    y: f32,
    z: f32
}

component Model {
    mesh: string
}

component RenderLayer {
    layer: RenderLayerType = RenderLayerType.Level
}

component Visible {
    is_visible: bool = true
}

component EventHandler {
    handles_collision: bool = true,
    handles_mouse: bool = true
}

component CoordinateSystem {
    system: CoordSystemType = CoordSystemType.World
}

// System to render level entities
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

### Example: View Entity (Modern ECS)

```heidic
// Create a view entity (3D panel element)
component ViewPosition {
    x: f32,
    y: f32,
    z: f32
}

component Sprite {
    texture: string
}

component RenderLayer {
    layer: RenderLayerType = RenderLayerType.ScreenForeground
}

component Visible {
    is_visible: bool = false  // Default hidden, set SHOW flag to true
}

component CoordinateSystem {
    system: CoordSystemType = CoordSystemType.View
}

component VisualOrder {
    order_type: OrderType = OrderType.Layer,
    layer: i32 = 0
}

// System to render view entities
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

### Example: Sky Entity (Modern ECS)

```heidic
// Create a sky entity (sky, background, horizon)
component SkyPosition {
    view_x: f32,
    view_y: f32,
    view_z: f32,
    world_rotation_x: f32,
    world_rotation_y: f32,
    world_rotation_z: f32
}

component SkyModel {
    mesh: string
}

component RenderLayer {
    layer: RenderLayerType = RenderLayerType.LevelBackground
}

component Visible {
    is_visible: bool = false  // Default hidden, set SHOW flag to true
}

component CoordinateSystem {
    system: CoordSystemType = CoordSystemType.Mixed
}

component VisualOrder {
    order_type: OrderType = OrderType.Both,
    layer: i32 = 0,
    view_distance: f32 = 0.0
}

// System to render sky entities
@system(render_sky, before = render_level)
fn render_sky_entities(q: query<SkyPosition, SkyModel, RenderLayer, Visible, VisualOrder>): void {
    for entity in q {
        if entity.RenderLayer.layer == RenderLayerType.LevelBackground && entity.Visible.is_visible {
            // Render sky with mixed coordinates
            draw_sky_model(entity.SkyModel.mesh, entity.SkyPosition, entity.VisualOrder);
        }
    }
}
```

---

## Comparison: 2001 vs Modern ECS

### 2001 Approach
```
Entity Type → Determines everything
- Creation method
- Rendering context
- Coordinate system
- Event handling
- Visibility flags
```

**Pros:**
- Simple to understand
- Clear categories
- Easy to implement

**Cons:**
- Rigid (can't change entity type)
- Inconsistent (different flags for visibility)
- Mixed concerns (rendering + creation + events)
- Hard to extend (new category = new entity type)

### Modern ECS Approach
```
Components → Flexible composition
- RenderLayer component (where to render)
- Visible component (visibility)
- CoordinateSystem component (coordinate space)
- EventHandler component (what events to handle)
- VisualOrder component (rendering order)
```

**Pros:**
- Flexible (add/remove components to change behavior)
- Consistent (same component pattern everywhere)
- Separated concerns (rendering, events, coordinates are separate)
- Easy to extend (new component = new feature)

**Cons:**
- More complex (need to understand component composition)
- More setup (need to add multiple components)

---

## Verdict: Is the 2001 Breakdown Good?

### For 2001: **Yes, it's reasonable**
- Clear separation of concerns
- Makes sense for the era
- Practical for game development

### For 2025: **Can be improved with ECS**
- Use components instead of entity types
- More flexible and extensible
- Better separation of concerns
- Easier to add new features

---

## Recommendation for HEIDIC

**Use component-based approach:**

1. **Don't create entity "types"** - create entities and add components
2. **Use `RenderLayer` component** - determines where entity renders
3. **Use `Visible` component** - consistent visibility handling
4. **Use `CoordinateSystem` component** - determines coordinate space
5. **Use `EventHandler` component** - determines what events entity handles
6. **Use `VisualOrder` component** - determines rendering order

**Example:**
```heidic
// Create a level entity (old way: ent_create with ENTITY* struct)
// New way: create entity, add components
let actor = create_entity();
add_component(actor, Position { x: 0, y: 0, z: 0 });
add_component(actor, Model { mesh: "actor.obj" });
add_component(actor, RenderLayer { layer: RenderLayerType.Level });
add_component(actor, Visible { is_visible: true });
add_component(actor, EventHandler { handles_collision: true, handles_mouse: true });
add_component(actor, CoordinateSystem { system: CoordSystemType.World });
```

**Benefits:**
- Entity can change category by adding/removing components
- Consistent component pattern
- Easy to query: `query<RenderLayer, Visible>` finds all visible entities in a layer
- Flexible: entity can have multiple render layers if needed

---

*The 2001 breakdown was good for its time, but modern ECS with components is more flexible and powerful.*

