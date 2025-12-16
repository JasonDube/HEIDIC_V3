# Resource System Implementation Order

## The Right Order

**Step 1:** Define Resource Types (EDEN Engine Runtime - C++)
**Step 2:** Implement Resource Handles (HEIDIC Language Syntax)

We need the C++ classes first, then the HEIDIC syntax generates code that uses them.

---

## Step 1: Define Resource Types (EDEN Engine Runtime)

### What We Need to Build First

#### 1.1 DDS Loader (stdlib/dds_loader.h)
```cpp
// Parse DDS headers, extract compressed data
struct DDSData {
    VkFormat format;      // BC7, BC5, R8, etc.
    uint32_t width, height;
    uint32_t mipmapCount;
    std::vector<uint8_t> compressedData;  // Ready for GPU upload
};

DDSData load_dds(const std::string& path);
```

#### 1.2 PNG Loader (stdlib/png_loader.h)
```cpp
// For source files (slower, but needed)
struct PNGData {
    uint32_t width, height;
    uint32_t channels;  // 3 or 4
    std::vector<uint8_t> pixels;  // RGBA8
};

PNGData load_png(const std::string& path);
```

#### 1.3 Texture Resource Class (stdlib/texture_resource.h)
```cpp
class TextureResource {
public:
    VkImage image;
    VkImageView imageView;
    VkSampler sampler;
    VkDeviceMemory memory;
    uint32_t width, height;
    VkFormat format;
    
    TextureResource(const std::string& path);
    ~TextureResource();
    
    void reload();  // For CONTINUUM hot-reload
    
private:
    void load_dds_texture(const std::string& path);
    void load_png_texture(const std::string& path);
};
```

#### 1.4 Mesh Resource Class (stdlib/mesh_resource.h)
```cpp
class MeshResource {
public:
    VkBuffer vertexBuffer;
    VkBuffer indexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkDeviceMemory indexBufferMemory;
    uint32_t indexCount;
    
    MeshResource(const std::string& path);
    ~MeshResource();
    
private:
    void load_obj_mesh(const std::string& path);
    // Future: void load_gltf_mesh(const std::string& path);
};
```

#### 1.5 Resource Base Template (stdlib/resource.h)
```cpp
// Base class for all resources (hot-reload, RAII)
template<typename T>
class Resource {
private:
    std::unique_ptr<T> data;
    std::string path;
    std::time_t lastModified;
    
public:
    Resource(const std::string& path);
    T* get() { return data.get(); }
    T& operator*() { return *data; }
    
    void reload();  // Check file modification time, reload if changed
};
```

---

## Step 2: Implement Resource Handles (HEIDIC Language)

### What We Add to HEIDIC Compiler

#### 2.1 Lexer: Add `resource` Keyword
```rust
// src/lexer.rs
Token::Resource  // New token
```

#### 2.2 Parser: Parse Resource Declarations
```rust
// src/parser.rs
fn parse_resource(&mut self) -> Result<Item> {
    self.expect(Token::Resource)?;
    let name = self.expect_ident()?;
    self.expect(Token::Colon)?;
    let resource_type = self.parse_type()?;  // Texture, Mesh, etc.
    self.expect(Token::Eq)?;
    let path = self.expect_string()?;  // "path/to/file.dds"
    self.expect(Token::Semicolon)?;
    
    Ok(Item::Resource { name, resource_type, path })
}
```

#### 2.3 AST: Resource Node
```rust
// src/ast.rs
pub enum Item {
    // ... existing items
    Resource {
        name: String,
        resource_type: Type,
        path: String,
    },
}
```

#### 2.4 Codegen: Generate Resource Code
```rust
// src/codegen.rs
fn generate_resource(&mut self, item: &Resource) -> String {
    match item.resource_type {
        Type::Texture => {
            format!(r#"
// Resource: {}
Resource<TextureResource> g_resource_{}("{}");
"#, item.name, item.name.to_lowercase(), item.path)
        },
        Type::Mesh => {
            format!(r#"
// Resource: {}
Resource<MeshResource> g_resource_{}("{}");
"#, item.name, item.name.to_lowercase(), item.path)
        },
    }
}
```

---

## Implementation Checklist

### Phase 1: Engine Runtime (Do This First!)

- [ ] **1.1** Create `stdlib/dds_loader.h` with DDS parser
  - Parse DDS header
  - Extract BC7/BC5/R8 format
  - Read compressed data blocks
  
- [ ] **1.2** Create `stdlib/png_loader.h` with PNG loader
  - Use stb_image (or write minimal loader)
  - Load RGBA8 pixels
  
- [ ] **1.3** Create `stdlib/texture_resource.h` with TextureResource class
  - Constructor loads DDS or PNG
  - Creates Vulkan image/imageView/sampler
  - RAII cleanup in destructor
  - Hot-reload support
  
- [ ] **1.4** Create `stdlib/mesh_resource.h` with MeshResource class
  - OBJ loader integration
  - Creates vertex/index buffers
  - RAII cleanup
  
- [ ] **1.5** Create `stdlib/resource.h` with Resource template
  - Generic wrapper for all resources
  - File watching for hot-reload
  - RAII lifecycle management

### Phase 2: HEIDIC Language (Do This Second!)

- [ ] **2.1** Add `resource` keyword to lexer
  - Token::Resource

- [ ] **2.2** Add resource parsing to parser
  - Parse `resource Name: Type = "path";`
  
- [ ] **2.3** Add Resource AST node
  - Store name, type, path
  
- [ ] **2.4** Generate Resource code in codegen
  - Generate `Resource<TextureResource> g_resource_name("path");`
  - Include proper headers (`#include "stdlib/resource.h"`)

### Phase 3: Integration & Testing

- [ ] **3.1** Test DDS texture loading manually (C++)
  - Load a DDS file
  - Verify Vulkan image creation
  - Verify GPU upload works

- [ ] **3.2** Test HEIDIC resource syntax
  - Write `resource Texture = "textures/brick.dds";`
  - Verify generated C++ compiles
  - Verify texture loads correctly

- [ ] **3.3** Add CONTINUUM hot-reload
  - Register resources for file watching
  - Reload on file change

---

## Example: Manual Test (Before HEIDIC Syntax)

### C++ Test (Step 1 - Engine Runtime)

```cpp
// test_texture.cpp
#include "stdlib/texture_resource.h"

int main() {
    // Manual test - verify TextureResource works
    TextureResource texture("textures/brick.dds");
    
    // Use texture
    // bind_texture(texture.imageView, texture.sampler);
    
    return 0;
}
```

Once this works, we know the engine runtime is ready.

### HEIDIC Test (Step 2 - Language Syntax)

```heidic
// test.hd
resource Texture = "textures/brick.dds";

fn main(): void {
    // Use texture (generates code using TextureResource)
    draw_texture(Texture);
}
```

This generates code that uses the TextureResource class we built in Step 1.

---

## Why This Order Matters

**Wrong Order (Trying HEIDIC syntax first):**
```rust
// Codegen tries to generate:
Resource<TextureResource> g_texture("brick.dds");

// But TextureResource doesn't exist yet!
// Compilation fails ❌
```

**Right Order (Engine runtime first):**
1. Build `TextureResource` class in C++ ✅
2. Test it manually ✅
3. Then HEIDIC codegen uses it ✅

---

## Next Steps: Start with Step 1.1

**Start Here:** `stdlib/dds_loader.h`

1. Write DDS header parser
2. Extract format (BC7, BC5, R8)
3. Read compressed data blocks
4. Test with a real DDS file

Once DDS loading works, move to Step 1.3 (TextureResource class), then we can implement the HEIDIC syntax that uses it!

