# HEIDIC Script to Test DDS Loader

## Setup

1. **Create a new project** in H_SCRIBE (name it whatever you want, e.g., "dds_test")
2. **Place a DDS texture** in the `textures/` folder (e.g., `textures/test.dds`)
3. **Copy compiled shaders** to your project:
   - Copy `examples/dds_quad_test/quad.vert.spv` → `shaders/quad.vert.spv` (in your project)
   - Copy `examples/dds_quad_test/quad.frag.spv` → `shaders/quad.frag.spv` (in your project)

## HEIDIC Script

Replace the contents of your `.hd` file with this:

```heidic
// HEIDIC Project: DDS Texture Test
// Loads and displays a DDS texture on a fullscreen quad

extern fn heidic_glfw_vulkan_hints(): void;
extern fn heidic_init_renderer_dds_quad(window: GLFWwindow, ddsPath: string): i32;
extern fn heidic_render_dds_quad(window: GLFWwindow): void;
extern fn heidic_cleanup_renderer_dds_quad(): void;
extern fn heidic_sleep_ms(milliseconds: i32): void;

fn main(): void {
    print("=== DDS Texture Test ===\n");
    print("Initializing GLFW...\n");

    let init_result: i32 = glfwInit();
    if init_result == 0 {
        print("Failed to initialize GLFW!\n");
        return;
    }

    print("GLFW initialized.\n");
    heidic_glfw_vulkan_hints();
    
    print("Creating window (800x600)...\n");
    let window: GLFWwindow = glfwCreateWindow(800, 600, "DDS Texture Test", 0, 0);
    if window == 0 {
        print("Failed to create window!\n");
        glfwTerminate();
        return;
    }

    print("Window created.\n");
    print("Initializing DDS quad renderer...\n");

    // Initialize renderer and load DDS texture
    // Path is relative to project directory (where .hd file is)
    // Note: String literals automatically convert to const char* for extern functions
    let renderer_init: i32 = heidic_init_renderer_dds_quad(window, "textures/test.dds");
    if renderer_init == 0 {
        print("Failed to initialize DDS renderer!\n");
        print("Make sure:\n");
        print("  1. textures/test.dds exists\n");
        print("  2. shaders/quad.vert.spv and shaders/quad.frag.spv exist\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return;
    }

    print("Renderer initialized!\n");
    print("Texture loaded successfully!\n");
    print("Starting render loop...\n");
    print("Press ESC or close the window to exit.\n");

    while glfwWindowShouldClose(window) == 0 {
        glfwPollEvents();

        if glfwGetKey(window, 256) == 1 { // ESC key
            glfwSetWindowShouldClose(window, 1);
        }

        // Render the DDS texture on fullscreen quad
        heidic_render_dds_quad(window);
        heidic_sleep_ms(16); // ~60 FPS cap
    }

    print("Cleaning up...\n");
    heidic_cleanup_renderer_dds_quad();
    glfwDestroyWindow(window);
    glfwTerminate();
    print("Program exited successfully.\n");
    print("Done!\n");
}
```

## What This Does

1. **Initializes GLFW** - Window creation
2. **Loads DDS texture** - `heidic_init_renderer_dds_quad()` loads `textures/test.dds`
3. **Renders fullscreen quad** - `heidic_render_dds_quad()` displays the texture each frame
4. **Displays texture** - You'll see your DDS texture covering the entire window!

## Testing Steps

1. **Get a DDS file:**
   - Convert a PNG to DDS using `texconv.exe` (from DirectXTex)
   - OR use an existing DDS file
   - Place it in `textures/test.dds`

2. **Copy shaders:**
   - Copy `quad.vert.spv` and `quad.frag.spv` to your project's `shaders/` folder

3. **Run the project:**
   - Click the Run button in H_SCRIBE
   - You should see your DDS texture displayed!

## Troubleshooting

- **"Failed to load DDS file"**: Make sure `textures/test.dds` exists
- **"Could not find quad shaders"**: Make sure `shaders/quad.vert.spv` and `shaders/quad.frag.spv` exist
- **Black screen**: Check that the DDS format is supported (BC7, BC5, etc.)

## Next Steps

Once this works, we've verified:
- ✅ DDS loader works correctly
- ✅ Texture upload to GPU works
- ✅ Fullscreen quad rendering works

Then we can move to Phase 1.3: TextureResource class!

