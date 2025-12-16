# Hot-Reloading Explained

## What is Hot-Reloading?

**Hot-reloading** (also called "live reloading" or "hot swapping") is a development feature that allows you to modify code while your program is running, and see those changes take effect **immediately** without restarting the application.

Think of it like editing a webpage in your browser's developer tools - you change the CSS, and the page updates instantly. Hot-reloading does the same thing, but for your game code.

---

## The Traditional Workflow (Without Hot-Reloading)

**Without hot-reloading, developing a game looks like this:**

1. Write code in your editor
2. Save the file
3. **Stop the game** (close the window)
4. Recompile the code
5. **Restart the game** (wait for window to open, load assets, etc.)
6. Navigate back to where you were testing
7. Test your change
8. If it's wrong, go back to step 1

**Time per iteration:** 30-60 seconds (or more if you have slow asset loading)

---

## The Hot-Reloading Workflow

**With hot-reloading, it looks like this:**

1. Write code in your editor
2. Save the file
3. **Game automatically reloads the code** (window stays open!)
4. Test your change immediately

**Time per iteration:** 2-5 seconds

---

## Real-World Example

### Scenario: You're tweaking a physics system

**Without hot-reloading:**
```
You: "Let me change the gravity constant from 9.8 to 10.0"
[Save file]
[Stop game - window closes]
[Recompile - 5 seconds]
[Restart game - 10 seconds]
[Wait for assets to load - 5 seconds]
[Navigate back to test scene - 5 seconds]
Total: 25 seconds just to test one number change
```

**With hot-reloading:**
```
You: "Let me change the gravity constant from 9.8 to 10.0"
[Save file]
[Game detects change - 1 second]
[Code reloads automatically]
[You see the change immediately]
Total: 1-2 seconds
```

---

## What Can Be Hot-Reloaded?

Different systems support hot-reloading different things:

### 1. **Shaders** (Most Common)
- Edit a shader file → see visual changes instantly
- Very common in game engines (Unity, Unreal, Godot all do this)
- **Why it's easy:** Shaders are just data files, not code

### 2. **Game Logic / Systems** (The Holy Grail)
- Edit your physics system → changes apply immediately
- Edit your rendering system → changes apply immediately
- **Why it's hard:** Requires dynamic library loading and state management

### 3. **Component Definitions** (Advanced)
- Change a component's fields → existing entities migrate to new layout
- **Why it's very hard:** Need to preserve entity data during layout changes

### 4. **Assets** (Common)
- Replace a texture → see it update in-game
- Replace a model → see it update in-game
- **Why it's easy:** Just reload the file

---

## How Does Hot-Reloading Work? (The Technical Side)

### Concept 1: Dynamic Library Loading

Your game code gets compiled into a **dynamic library** (`.dll` on Windows, `.so` on Linux, `.dylib` on macOS) instead of being baked into the main executable.

```
Main Game Executable (never changes)
    ↓
    Loads → Game Logic DLL (can be reloaded)
```

When you change code:
1. Compiler rebuilds the DLL
2. Game detects the DLL changed (file watcher)
3. Game unloads the old DLL
4. Game loads the new DLL
5. Game updates function pointers to point to new code

### Concept 2: State Preservation

The tricky part: **What happens to your game state?**

**Example:** You have 100 entities with Position and Velocity components. You modify the physics system code. What happens?

**Bad approach:** Lose all entities, restart from scratch
**Good approach:** Keep all entities, just swap out the physics function

**The solution:** Separate "data" (entities, components) from "code" (systems). Only reload the code, keep the data.

### Concept 3: File Watching

The game needs to know when files change. This is done with a **file watcher**:

- Windows: `ReadDirectoryChangesW` API
- Linux: `inotify` API  
- macOS: `FSEvents` API

Or use a cross-platform library like Rust's `notify` crate.

---

## Hot-Reloading in HEIDIC

### The Vision

In HEIDIC, you'd mark code as "hot-reloadable" with an attribute:

```heidic
@hot
system(physics) {
    fn update(q: query<Position, Velocity>): void {
        let gravity: f32 = 9.8;  // Change this number
        for entity in q {
            entity.Velocity.y = entity.Velocity.y - gravity * 0.016;
            entity.Position.y = entity.Position.y + entity.Velocity.y * 0.016;
        }
    }
}
```

**What happens:**
1. You edit the `gravity` value from `9.8` to `10.0`
2. You save the file
3. HEIDIC compiler detects the change
4. Compiler rebuilds just that system
5. Game automatically reloads the new code
6. Your entities keep moving, but now with the new gravity value
7. **Window never closes, game never stops**

### What Gets Hot-Reloaded?

**Phase 1 (Easy):**
- ✅ Shaders (already supported via file watching)
- ✅ Systems marked with `@hot`

**Phase 2 (Medium):**
- ✅ Component definitions (with entity migration)
- ✅ Functions marked with `@hot`

**Phase 3 (Advanced):**
- ✅ Resource definitions
- ✅ Pipeline definitions

---

## Why Is Hot-Reloading a "Killer Feature"?

### 1. **Speed of Iteration**

Traditional workflow: 30-60 seconds per change
Hot-reload workflow: 1-5 seconds per change

**10-20x faster iteration** = 10-20x more experiments = better games

### 2. **Preserve Context**

You don't lose your place:
- Camera position stays the same
- Entities stay in the same state
- You can test edge cases without re-creating them

### 3. **The "Flow State"**

When iteration is fast, you enter a flow state:
- Try something → see result → adjust → see result → adjust
- No interruptions from restarts
- Pure creative focus

### 4. **It's What Makes Jai Famous**

The Jai programming language became famous specifically because of its hot-reloading. People said:
> "I can't go back to C++ after using Jai's hot-reload"

It's that powerful.

---

## Real-World Use Cases

### Use Case 1: Tuning Gameplay Values

```heidic
@hot
system(combat) {
    fn damage_calculation(attacker: Entity, defender: Entity): void {
        let base_damage: f32 = 10.0;  // Tweak this
        let crit_multiplier: f32 = 2.0;  // Tweak this
        // ... combat logic
    }
}
```

**Without hot-reload:** Change number → restart → load level → find enemy → test → repeat
**With hot-reload:** Change number → see result immediately → adjust → see result → done

### Use Case 2: Debugging Rendering

```heidic
@hot
shader fragment "shaders/pbr.frag" {
    // Edit shader code
    // See changes instantly without restarting
}
```

**Without hot-reload:** Edit shader → recompile → restart game → navigate to scene → test
**With hot-reload:** Edit shader → see change instantly

### Use Case 3: Prototyping Features

```heidic
@hot
system(experimental_feature) {
    // Try crazy ideas
    // See results immediately
    // Iterate rapidly
}
```

Perfect for game jams, rapid prototyping, and experimentation.

---

## Implementation Challenges

### Challenge 1: State Management

**Problem:** How do you preserve game state when reloading code?

**Solution:** 
- Keep all data (entities, components) in the main executable
- Only reload the code (systems, functions)
- Systems operate on data, they don't own it

### Challenge 2: Function Pointer Updates

**Problem:** Old code has function pointers to old functions. How do you update them?

**Solution:**
- Use function tables (arrays of function pointers)
- When reloading, update the table entries
- All code calls through the table, not directly

### Challenge 3: Component Layout Changes

**Problem:** What if you change a component's fields?

**Example:**
```heidic
// Old version
component Transform {
    x: f32,
    y: f32
}

// New version (added z field)
component Transform {
    x: f32,
    y: f32,
    z: f32  // New field!
}
```

**Solution:**
- Migration system that converts old data to new format
- Default values for new fields
- Version tracking for components

### Challenge 4: Platform Differences

**Problem:** Different platforms have different APIs for dynamic loading

**Solution:**
- Windows: `LoadLibrary` / `FreeLibrary` / `GetProcAddress`
- Linux: `dlopen` / `dlclose` / `dlsym`
- macOS: `NSModule` / `dyld` APIs
- Use a cross-platform wrapper library

---

## Comparison: Hot-Reload vs Other Approaches

### Hot-Reload vs Scripting Languages

**Scripting (Lua, Python):**
- ✅ Easy to reload (just re-execute script)
- ❌ Slower performance
- ❌ Less type safety

**Hot-Reload (Compiled Code):**
- ✅ Full performance (native code)
- ✅ Full type safety
- ❌ More complex to implement

### Hot-Reload vs Separate Tools

**External Tools (Level Editor, Script Editor):**
- ✅ Can edit while game runs
- ❌ Requires separate tool
- ❌ Data sync issues

**Hot-Reload:**
- ✅ Everything in one place (your code editor)
- ✅ No tool switching
- ✅ Direct code editing

---

## The HEIDIC Implementation Plan

### Phase 1: Basic Hot-Reload (Week 1)

**Goal:** Hot-reload systems marked with `@hot`

**Steps:**
1. Add `@hot` attribute parsing to lexer/parser
2. Generate systems as separate DLLs
3. Implement file watcher for `.hd` files
4. Implement DLL reloading (Windows first)
5. Update function pointers on reload

**Result:** You can edit system code and see changes instantly

### Phase 2: Shader Hot-Reload (Week 1-2)

**Goal:** Hot-reload shaders (already partially supported)

**Steps:**
1. Detect shader file changes
2. Recompile shader to SPIR-V
3. Rebuild pipeline with new shader
4. Swap pipelines seamlessly

**Result:** Edit shader → see visual changes instantly

### Phase 3: Component Migration (Week 2)

**Goal:** Hot-reload component definitions with data migration

**Steps:**
1. Track component versions
2. Generate migration code
3. Migrate entities on component change
4. Preserve existing data

**Result:** Change component fields → entities migrate automatically

---

## Example: Full Hot-Reload Workflow

### Step 1: Write Code

```heidic
@hot
system(physics) {
    fn update(q: query<Position, Velocity>): void {
        let gravity: f32 = 9.8;
        for entity in q {
            entity.Velocity.y = entity.Velocity.y - gravity * 0.016;
            entity.Position.y = entity.Position.y + entity.Velocity.y * 0.016;
        }
    }
}
```

### Step 2: Run Game

```
heidic_v2 run game.hd
```

Game starts, window opens, entities are moving with gravity = 9.8

### Step 3: Edit Code

You change `gravity` from `9.8` to `10.0` in your editor and save.

### Step 4: Automatic Reload

```
[File watcher detects change]
[Compiler rebuilds physics system DLL]
[Game unloads old DLL]
[Game loads new DLL]
[Game updates function pointers]
[Physics system now uses gravity = 10.0]
```

**Total time:** 1-2 seconds

**Result:** Entities immediately start falling faster. Window never closed. Game never stopped.

---

## Why This Matters for HEIDIC

### The Competitive Advantage

Most game engines either:
- Don't support hot-reload at all (C++ engines)
- Only support shader hot-reload (Unity, Unreal)
- Require scripting languages for hot-reload (slower)

**HEIDIC would be one of the few** that supports:
- ✅ Full system hot-reload (native code, full performance)
- ✅ Shader hot-reload
- ✅ Component hot-reload with migration
- ✅ All in one language, no scripting needed

### The Developer Experience

Hot-reloading transforms HEIDIC from "a good language" to "the language I can't live without."

It's the difference between:
- "This is a nice compiler" 
- "I can't go back to anything else"

---

## Summary

**Hot-reloading** = Edit code while game runs → see changes instantly → never restart

**Why it's powerful:**
- 10-20x faster iteration
- Preserves context (don't lose your place)
- Enables flow state (pure creative focus)
- Industry-changing feature (made Jai famous)

**What HEIDIC would support:**
- Systems (`@hot system`)
- Shaders (`@hot shader`)
- Components (with automatic migration)
- All in native code, full performance

**The result:**
The fastest-iterating game development experience possible.

---

*"The best tool is the one that gets out of your way and lets you create."* - Hot-reloading does exactly that.

