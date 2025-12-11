# Component Hot-Loading Implementation Plan

## Status: Foundation Complete ✅

### Completed:
- ✅ Parser supports `@hot component` syntax
- ✅ AST has `is_hot` field on `ComponentDef`
- ✅ Codegen tracks hot components

### Next Steps:

## Phase 1: Component Metadata Generation

**Goal:** Generate metadata for each `@hot` component that tracks:
- Component version (increments on layout change)
- Field layout (names, types, offsets)
- Default values

**Implementation:**
```cpp
// Generated for each @hot component
struct ComponentMetadata_Transform {
    static constexpr const char* name = "Transform";
    static constexpr uint32_t version = 1;
    static constexpr size_t size = sizeof(Transform);
    // Field info...
};
```

## Phase 2: Version Tracking System

**Goal:** Track component versions at runtime

**Implementation:**
```cpp
std::unordered_map<std::string, uint32_t> g_component_versions;

void init_component_versions() {
    g_component_versions["Transform"] = 1;
    // ...
}
```

## Phase 3: Migration Function Generation

**Goal:** Generate functions to migrate from old to new component layouts

**Implementation:**
```cpp
// When layout changes from v1 to v2:
Transform_v2 migrate_Transform_v1_to_v2(const Transform_v1& old) {
    Transform_v2 new_comp;
    new_comp.position = old.position;  // Unchanged field
    new_comp.rotation = vec3_to_quat(old.rotation);  // Type conversion
    new_comp.scale = Vec3(1.0, 1.0, 1.0);  // New field default
    return new_comp;
}
```

## Phase 4: Runtime Migration Loop

**Goal:** Check component versions and migrate entities at runtime

**Implementation:**
```cpp
void check_and_migrate_hot_components() {
    // Check if any hot component layouts changed
    // For each component that changed:
    //   - Load old component data for all entities
    //   - Run migration function
    //   - Store new component data
    //   - Update version number
}
```

## Phase 5: Python Editor Integration

**Goal:** Detect component changes and trigger hot-reload

**Implementation:**
- Watch for `.hd` file changes
- Recompile HEIDIC → C++
- Detect component layout changes
- Trigger component migration at runtime

## Current Progress

- ✅ Foundation (parser, AST)
- 🔄 Starting codegen (metadata generation)

