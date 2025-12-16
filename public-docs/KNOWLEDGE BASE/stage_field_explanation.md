# Shader `stage` Field Analysis

## The Issue

The `ShaderDef` struct in `src/ast.rs` has a `stage: ShaderStage` field that is:
- ✅ **Set correctly** during parsing (from keywords like `vertex`, `fragment`)
- ❌ **Never read or used** anywhere in the codebase

Instead, **everything derives the shader stage from the file extension**.

## Current Flow

### 1. Parsing (Rust) - Sets `stage` field
```rust
// src/parser.rs
fn parse_shader(&mut self, is_hot: bool) -> Result<ShaderDef> {
    let stage = match self.peek() {
        Token::Vertex => ShaderStage::Vertex,     // ✅ Parsed correctly
        Token::Fragment => ShaderStage::Fragment,  // ✅ Parsed correctly
        // ...
    };
    
    let path = /* parse path string */;
    
    Ok(ShaderDef { stage, path, is_hot })  // ✅ Field is set
}
```

**HEIDIC code:**
```heidic
@hot
shader vertex "shaders/my_shader.vert" { }  // stage = Vertex
```

### 2. Code Generation (Rust) - Ignores `stage` field
```rust
// src/codegen.rs
for shader in self.hot_shaders.iter() {
    let shader_path = &shader.path;  // ✅ Uses path
    // shader.stage is NEVER read! ❌
    
    // Instead, derives stage from file extension:
    // ".vert" -> vertex
    // ".frag" -> fragment
}
```

### 3. Shader Compilation (Python) - Derives from extension
```python
# H_SCRIBE/main.py
if shader_path.endswith('.vert'):
    stage_flag = "-fshader-stage=vertex"  # Derived from extension
elif shader_path.endswith('.frag'):
    stage_flag = "-fshader-stage=fragment"  # Derived from extension
```

### 4. Runtime (C++) - Derives from extension
```cpp
// vulkan/eden_vulkan_helpers.cpp
bool isVertex = (shaderPathStr.find(".vert") != std::string::npos);  // String matching
bool isFragment = (shaderPathStr.find(".frag") != std::string::npos);  // String matching
```

## The Problem

**Redundancy:** You have **two sources of truth** for shader stage:
1. **Explicit keyword** in HEIDIC code: `shader vertex "file.vert"`
2. **File extension**: `.vert` = vertex, `.frag` = fragment

Currently, only #2 is used. The explicit keyword is parsed but ignored.

## Potential Issues

1. **Mismatch not caught:**
   ```heidic
   shader fragment "shaders/vertex_shader.vert" { }  // Wrong! Says fragment but file is .vert
   ```
   - Parser sets `stage = Fragment`
   - But runtime uses `.vert` extension → treats as vertex
   - **No error caught!**

2. **Ambiguous file extensions:**
   - What if someone uses `.glsl` for everything?
   - The explicit stage keyword would be the only way to know

3. **Type safety lost:**
   - The Rust type system knows the stage, but we throw it away

## Options

### Option A: Use the `stage` field (Recommended)

**Pros:**
- ✅ Single source of truth (the keyword)
- ✅ Catches mismatches at compile-time
- ✅ Works with any file extension (`.glsl`, `.shader`, etc.)
- ✅ More explicit and type-safe

**Cons:**
- ❌ Need to pass stage info through the pipeline
- ❌ More code changes needed

**Implementation:**
1. In codegen: Generate stage info in C++ code
2. In Python: Use the stage from AST (would need to parse HEIDIC)
3. In C++: Accept stage as parameter or embed in generated code

### Option B: Remove the `stage` field

**Pros:**
- ✅ Simpler - one less field to track
- ✅ File extension is clear and conventional
- ✅ No changes needed to existing code

**Cons:**
- ❌ Less flexible (must use conventional extensions)
- ❌ Can't catch keyword/extension mismatches
- ❌ Can't use `.glsl` files easily

**Implementation:**
1. Remove `stage` field from `ShaderDef`
2. Remove `ShaderStage` enum if not needed elsewhere
3. Update parser to not parse the keyword (or keep it for validation only)

### Option C: Keep field but use it for validation

**Pros:**
- ✅ Best of both worlds
- ✅ Validates keyword matches extension
- ✅ Can still use extension in runtime

**Cons:**
- ❌ Field still marked as "unused" (only used in validation)
- ❌ Adds validation complexity

**Implementation:**
1. Add validation in type checker: `if stage == Vertex && !path.ends_with(".vert")` → error
2. Still derive stage from extension at runtime

## Recommendation

**Option C (Validation)** seems best:
- Keep the explicit keyword for clarity and validation
- Use file extension for runtime (simpler, no changes needed)
- Add validation to catch mismatches

This way:
- The `stage` field is used (for validation) → no warning
- You get safety (mismatches caught)
- Runtime stays simple (extension-based)

**Alternative:** If validation isn't needed, **Option B (Remove)** is simplest.

What would you prefer?

