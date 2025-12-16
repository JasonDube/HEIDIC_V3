# ECS Queries Explained - What "Query-Based" Means

## What is Query-Based ECS?

**Query-based ECS** (Entity Component System) is a pattern where you "query" for entities that have specific combinations of components. It's similar to SQL in concept, but very different in practice.

### SQL Analogy (But Not SQL!)

Think of it like this:

**SQL:**
```sql
SELECT * FROM users WHERE age > 18 AND city = 'New York';
```
"Give me all users who are over 18 AND live in New York"

**ECS Query:**
```heidic
query<Position, Velocity>
```
"Give me all entities that have BOTH a Position component AND a Velocity component"

### The Key Difference

- **SQL:** Queries data in a database (rows and columns)
- **ECS Query:** Queries entities in a game world (objects with components)

---

## How ECS Works

### Entities, Components, Systems

**Entity:** An object in your game (a player, enemy, bullet, etc.)
- Just an ID number (like `Entity #42`)

**Component:** A piece of data attached to an entity
- `Position { x: f32, y: f32, z: f32 }` - where something is
- `Velocity { x: f32, y: f32, z: f32 }` - how fast it's moving
- `Health { current: i32, max: i32 }` - how much health it has

**System:** Code that processes entities with specific components
- Physics system: processes entities with `Position` + `Velocity`
- Render system: processes entities with `Position` + `Mesh`

### Example: A Game World

```
Entity #1: Player
  - Position { x: 10, y: 5, z: 0 }
  - Velocity { x: 1, y: 0, z: 0 }
  - Health { current: 100, max: 100 }
  - Mesh { model: "player.obj" }

Entity #2: Enemy
  - Position { x: 20, y: 5, z: 0 }
  - Velocity { x: -1, y: 0, z: 0 }
  - Health { current: 50, max: 50 }
  - Mesh { model: "enemy.obj" }

Entity #3: Bullet
  - Position { x: 15, y: 5, z: 0 }
  - Velocity { x: 5, y: 0, z: 0 }
  - Mesh { model: "bullet.obj" }
  - (No Health component)
```

### Querying for Entities

**Query: `query<Position, Velocity>`**
- Matches: Entity #1 (Player), Entity #2 (Enemy), Entity #3 (Bullet)
- All three have both Position and Velocity

**Query: `query<Position, Health>`**
- Matches: Entity #1 (Player), Entity #2 (Enemy)
- Entity #3 (Bullet) doesn't have Health, so it's excluded

**Query: `query<Position, Velocity, Health>`**
- Matches: Entity #1 (Player), Entity #2 (Enemy)
- Entity #3 (Bullet) doesn't have Health, so it's excluded

---

## HEIDIC's Query Syntax

### Current (What We Have)

```heidic
component Position {
    x: f32,
    y: f32,
    z: f32
}

component Velocity {
    x: f32,
    y: f32,
    z: f32
}

fn update_physics(q: query<Position, Velocity>): void {
    // q contains all entities with BOTH Position AND Velocity
    // But... how do we iterate? This is what Sprint 1 fixes!
}
```

### What We Need (Sprint 1)

```heidic
fn update_physics(q: query<Position, Velocity>): void {
    for entity in q {
        // entity.Position and entity.Velocity are available
        entity.Position.x += entity.Velocity.x * delta_time;
        entity.Position.y += entity.Velocity.y * delta_time;
        entity.Position.z += entity.Velocity.z * delta_time;
    }
}
```

---

## Why "Query-Based" is Powerful

### 1. Automatic Filtering

You don't manually check "does this entity have Position and Velocity?" - the query does it for you.

**Without queries (manual):**
```c++
for (auto& entity : all_entities) {
    if (entity.has<Position>() && entity.has<Velocity>()) {
        // Update physics
    }
}
```

**With queries (automatic):**
```heidic
for entity in query<Position, Velocity> {
    // Only entities with BOTH components are here
    // No manual checking needed!
}
```

### 2. Cache-Friendly Iteration

ECS stores components in separate arrays (SOA - Structure of Arrays), which is cache-friendly:

```
Position components: [pos1, pos2, pos3, ...]  // All positions together
Velocity components: [vel1, vel2, vel3, ...]  // All velocities together
```

When you iterate `query<Position, Velocity>`, you're iterating through these arrays in order, which is very fast (cache-friendly).

### 3. System Organization

Each system processes entities with specific component combinations:

```heidic
// Physics system - only processes entities with Position + Velocity
@system(physics)
fn update_physics(q: query<Position, Velocity>): void {
    for entity in q {
        entity.Position += entity.Velocity * dt;
    }
}

// Render system - only processes entities with Position + Mesh
@system(render, after = physics)
fn render_entities(q: query<Position, Mesh>): void {
    for entity in q {
        draw_mesh(entity.Mesh, entity.Position);
    }
}
```

---

## Is HEIDIC a "Query-Based Language"?

**Not exactly.** HEIDIC is an **ECS-native language** that uses queries as a core feature.

**Better description:**
- HEIDIC is a **game engine language** with built-in ECS support
- ECS uses **query-based patterns** to find entities
- HEIDIC makes queries a **first-class language feature**

**Similar languages:**
- **Bevy (Rust):** ECS game engine with query syntax
- **Flecs (C):** ECS framework with query API
- **EnTT (C++):** ECS library with query views

**HEIDIC's advantage:**
- Queries are built into the language (not a library)
- Compile-time query generation (zero runtime overhead)
- Type-safe queries (compiler checks component types)

---

## Real-World Example

### A Simple Game Loop

```heidic
component Position { x: f32, y: f32, z: f32 }
component Velocity { x: f32, y: f32, z: f32 }
component Health { current: i32, max: i32 }
component Mesh { model: string }

// Physics system - updates positions based on velocity
@system(physics)
fn update_physics(q: query<Position, Velocity>): void {
    for entity in q {
        entity.Position.x += entity.Velocity.x * delta_time;
        entity.Position.y += entity.Velocity.y * delta_time;
        entity.Position.z += entity.Velocity.z * delta_time;
    }
}

// Damage system - reduces health over time
@system(damage, after = physics)
fn apply_damage(q: query<Health>): void {
    for entity in q {
        entity.Health.current -= 1; // Lose 1 HP per frame
        if entity.Health.current <= 0 {
            // Entity dies (remove components or mark for deletion)
        }
    }
}

// Render system - draws entities
@system(render, after = damage)
fn render_entities(q: query<Position, Mesh>): void {
    for entity in q {
        draw_mesh(entity.Mesh.model, entity.Position);
    }
}

fn main(): void {
    // Create some entities
    let player = create_entity();
    add_component(player, Position { x: 0, y: 0, z: 0 });
    add_component(player, Velocity { x: 1, y: 0, z: 0 });
    add_component(player, Health { current: 100, max: 100 });
    add_component(player, Mesh { model: "player.obj" });
    
    // Game loop
    while running {
        // Systems run in dependency order (physics → damage → render)
        run_systems();
    }
}
```

### What Happens

1. **Physics system** queries for entities with `Position + Velocity`
   - Finds: Player (has both)
   - Updates player's position based on velocity

2. **Damage system** queries for entities with `Health`
   - Finds: Player (has Health)
   - Reduces player's health

3. **Render system** queries for entities with `Position + Mesh`
   - Finds: Player (has both)
   - Draws player at current position

Each system only processes entities that match its query - automatic filtering!

---

## Summary

**Query-based ECS:**
- "Query" = find entities with specific component combinations
- Similar concept to SQL (filtering), but for game entities
- Very efficient (cache-friendly, automatic filtering)

**HEIDIC:**
- ECS-native language (ECS built into the language)
- Query syntax is a first-class feature
- Compile-time query generation (zero overhead)

**Sprint 1 Goal:**
- Add `for entity in q` iteration syntax
- Make queries actually usable (currently you can declare them but can't iterate)

---

*Now let's implement Sprint 1!*

