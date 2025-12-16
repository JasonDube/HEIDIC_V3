use anyhow::Result;
use crate::parser::{HeirocProgram, HeirocStmt, MainLoopParams, PanelDef};
use std::path::Path;
use std::fs;

pub struct CodeGenerator;

impl CodeGenerator {
    pub fn new() -> Self {
        Self
    }
    
    fn check_neuroshell_enabled(&self, project_dir: &Path) -> bool {
        let config_path = project_dir.join(".project_config");
        if let Ok(contents) = fs::read_to_string(&config_path) {
            for line in contents.lines() {
                let line = line.trim();
                if line.starts_with("enable_neuroshell=") {
                    let value = line.split('=').nth(1).unwrap_or("").trim().to_lowercase();
                    return value == "true" || value == "1" || value == "yes";
                }
            }
        }
        false
    }
    
    pub fn generate(&mut self, program: &HeirocProgram, project_dir: &Path) -> Result<String> {
        let neuroshell_enabled = self.check_neuroshell_enabled(project_dir);
        let mut output = String::new();
        
        // Generate includes and externs
        output.push_str("// HEIDIC code generated from HEIROC\n");
        
        // Only include NEUROSHELL if enabled in project config
        if neuroshell_enabled {
            output.push_str("// NEUROSHELL - Lightweight in-game UI system\n");
            output.push_str("extern fn neuroshell_init(window: GLFWwindow): i32;\n");
            output.push_str("extern fn neuroshell_update(delta_time: f32): void;\n");
            output.push_str("extern fn neuroshell_shutdown(): void;\n");
            output.push_str("extern fn neuroshell_is_enabled(): bool;\n");
            output.push_str("\n");
            output.push_str("// NEUROSHELL UI Element Creation\n");
            output.push_str("extern fn neuroshell_create_panel(x: f32, y: f32, width: f32, height: f32): i32;\n");
            output.push_str("extern fn neuroshell_set_visible(element_id: i32, visible: bool): void;\n");
            output.push_str("extern fn neuroshell_set_position(element_id: i32, x: f32, y: f32): void;\n");
            output.push_str("extern fn neuroshell_set_size(element_id: i32, width: f32, height: f32): void;\n");
            output.push_str("extern fn neuroshell_set_depth(element_id: i32, depth: f32): void;\n");
            output.push_str("\n");
        }
        output.push_str("extern fn heidic_glfw_vulkan_hints(): void;\n");
        output.push_str("extern fn heidic_init_renderer(window: GLFWwindow): i32;\n");
        output.push_str("extern fn heidic_render_frame(window: GLFWwindow): void;\n");
        output.push_str("extern fn heidic_cleanup_renderer(): void;\n");
        output.push_str("extern fn heidic_sleep_ms(milliseconds: i32): void;\n");
        // Declare heidic_load_level - it may be called if level file exists
        output.push_str("extern fn heidic_load_level(level_path: string): void;\n");
        output.push_str("extern fn glfwGetTime(): f64;\n");
        output.push_str("extern fn f32_to_i32(value: f32): i32;\n");
        // Window creation functions
        output.push_str("extern fn heidic_create_fullscreen_window(title: string): GLFWwindow;\n");
        output.push_str("extern fn heidic_create_borderless_window(width: i32, height: i32, title: string): GLFWwindow;\n");
        output.push_str("\n");
        
        // Generate main function (panels will be generated inside main)
        let main_loop_params = program.statements.iter()
            .find_map(|s| {
                if let HeirocStmt::MainLoop(params) = s {
                    Some(params)
                } else {
                    None
                }
            });
        
        if let Some(params) = main_loop_params {
            output.push_str(&self.generate_main_function(params, program, neuroshell_enabled, project_dir)?);
        } else {
            // Default main if no main_loop found
            output.push_str(&self.generate_main_function(&MainLoopParams {
                video_resolution: Some(1),
                video_mode: Some(0),
                fps_max: Some(60),
                random_seed: Some(0),
                load_level: Some("level.eden".to_string()),
            }, program, neuroshell_enabled, project_dir)?);
        }
        
        Ok(output)
    }
    
    fn generate_panel(&self, panel: &PanelDef) -> Result<String> {
        let mut output = String::new();
        
        // Extract panel properties
        let mut pos_x = 0.0;
        let mut pos_y = 0.0;
        let width = 100.0;
        let height = 20.0;
        let mut layer = 0.0;
        let mut visible = true;
        let mut bmap = String::new();
        
        for (name, value) in &panel.properties {
            match name.as_str() {
                "pos_x" => {
                    if let crate::parser::HeirocExpr::Integer(i) = value {
                        pos_x = *i as f32;
                    } else if let crate::parser::HeirocExpr::Number(n) = value {
                        pos_x = *n as f32;
                    }
                }
                "pos_y" => {
                    if let crate::parser::HeirocExpr::Integer(i) = value {
                        pos_y = *i as f32;
                    } else if let crate::parser::HeirocExpr::Number(n) = value {
                        pos_y = *n as f32;
                    }
                }
                "layer" => {
                    if let crate::parser::HeirocExpr::Integer(i) = value {
                        layer = *i as f32;
                    } else if let crate::parser::HeirocExpr::Number(n) = value {
                        layer = *n as f32;
                    }
                }
                "flags" => {
                    // TODO: Parse VISIBLE flag
                    visible = true;
                }
                "bmap" => {
                    if let crate::parser::HeirocExpr::String(s) = value {
                        bmap = s.clone();
                    }
                }
                _ => {}
            }
        }
        
        // Generate HEIDIC code for panel
        output.push_str(&format!("// Panel: {}\n", panel.name));
        output.push_str(&format!("let {}: i32 = neuroshell_create_panel({}, {}, {}, {});\n", 
            panel.name, pos_x, pos_y, width, height));
        output.push_str(&format!("neuroshell_set_depth({}, {});\n", panel.name, layer));
        if visible {
            output.push_str(&format!("neuroshell_set_visible({}, true);\n", panel.name));
        }
        if !bmap.is_empty() {
            output.push_str(&format!("// TODO: Set texture: {}\n", bmap));
        }
        
        Ok(output)
    }
    
    fn generate_main_function(&self, params: &MainLoopParams, program: &HeirocProgram, neuroshell_enabled: bool, project_dir: &Path) -> Result<String> {
        let mut output = String::new();
        
        // Resolution mapping: 0=640x480, 1=800x600, 2=1280x720, 3=1920x1080
        let (width, height) = match params.video_resolution.unwrap_or(1) {
            0 => (640, 600),
            1 => (800, 600),
            2 => (1280, 720),
            3 => (1920, 1080),
            _ => (800, 600),
        };
        
        let video_mode = params.video_mode.unwrap_or(0);
        let fps_max = params.fps_max.unwrap_or(60);
        let random_seed = params.random_seed.unwrap_or(0);
        let load_level = params.load_level.as_ref().map(|s| s.as_str()).unwrap_or("level.eden");
        
        output.push_str("fn main(): void {\n");
        output.push_str("    print(\"=== HEIROC Project ===\\n\");\n");
        output.push_str("    print(\"Initializing GLFW...\\n\");\n");
        output.push_str("\n");
        output.push_str("    let init_result: i32 = glfwInit();\n");
        output.push_str("    if init_result == 0 {\n");
        output.push_str("        print(\"Failed to initialize GLFW!\\n\");\n");
        output.push_str("        return;\n");
        output.push_str("    }\n");
        output.push_str("\n");
        output.push_str("    print(\"GLFW initialized.\\n\");\n");
        output.push_str("    heidic_glfw_vulkan_hints();\n");
        output.push_str("\n");
        
        // Create window based on video_mode: 0=Windowed, 1=Fullscreen, 2=Borderless
        match video_mode {
            1 => {
                // Fullscreen mode
                output.push_str("    print(\"Creating fullscreen window...\\n\");\n");
                output.push_str("    let window: GLFWwindow = heidic_create_fullscreen_window(\"HEIROC Project\");\n");
            },
            2 => {
                // Borderless mode
                output.push_str(&format!("    print(\"Creating borderless window ({}x{})...\\n\");\n", width, height));
                output.push_str(&format!("    let window: GLFWwindow = heidic_create_borderless_window({}, {}, \"HEIROC Project\");\n", width, height));
            },
            _ => {
                // Windowed mode (default)
                output.push_str(&format!("    print(\"Creating window ({}x{})...\\n\");\n", width, height));
                output.push_str(&format!("    let window: GLFWwindow = glfwCreateWindow({}, {}, \"HEIROC Project\", 0, 0);\n", width, height));
            }
        }
        
        output.push_str("    if window == 0 {\n");
        output.push_str("        print(\"Failed to create window!\\n\");\n");
        output.push_str("        glfwTerminate();\n");
        output.push_str("        return;\n");
        output.push_str("    }\n");
        output.push_str("\n");
        output.push_str("    print(\"Window created.\\n\");\n");
        output.push_str("    print(\"Initializing Vulkan renderer...\\n\");\n");
        output.push_str("\n");
        output.push_str("    let renderer_init: i32 = heidic_init_renderer(window);\n");
        output.push_str("    if renderer_init == 0 {\n");
        output.push_str("        print(\"Failed to initialize renderer!\\n\");\n");
        output.push_str("        glfwDestroyWindow(window);\n");
        output.push_str("        glfwTerminate();\n");
        output.push_str("        return;\n");
        output.push_str("    }\n");
        output.push_str("\n");
        output.push_str("    print(\"Renderer initialized!\\n\");\n");
        
        // Generate panel declarations inside main
        for stmt in &program.statements {
            if let HeirocStmt::Panel(panel) = stmt {
                output.push_str("    ");
                let panel_code = self.generate_panel(&panel)?;
                output.push_str(&panel_code.replace("\n", "\n    "));
                output.push_str("\n");
            }
        }
        
        // Only generate NEUROSHELL code if enabled
        if neuroshell_enabled {
            output.push_str("    // Initialize NEUROSHELL (if enabled)\n");
            output.push_str("    if (neuroshell_is_enabled()) {\n");
            output.push_str("        print(\"Initializing NEUROSHELL...\\n\");\n");
            output.push_str("        let neuroshell_init_result: i32 = neuroshell_init(window);\n");
            output.push_str("        if (neuroshell_init_result == 0) {\n");
            output.push_str("            print(\"WARNING: NEUROSHELL initialization failed!\\n\");\n");
            output.push_str("        } else {\n");
            output.push_str("            print(\"NEUROSHELL initialized!\\n\");\n");
            output.push_str("        }\n");
            output.push_str("    }\n");
            output.push_str("\n");
        }
        
        if random_seed != 0 {
            output.push_str(&format!("    // Random seed: {}\n", random_seed));
            output.push_str(&format!("    srand({});\n", random_seed));
            output.push_str("\n");
        }
        
        // Check if level file exists and provide friendly error message
        if !load_level.is_empty() {
            let level_path = project_dir.join(load_level);
            if !level_path.exists() {
                // Escape backslashes for C++ string literal
                let escaped_path = level_path.display().to_string().replace('\\', "\\\\");
                output.push_str(&format!("    // WARNING: Level file '{}' not found in project directory, skipping level load...\n", load_level));
                output.push_str(&format!("    print(\"WARNING: Level file '{}' not found in project directory, skipping level load...\\n\");\n", load_level));
                output.push_str(&format!("    print(\"  Expected location: {}\\n\");\n", escaped_path));
                output.push_str("    print(\"  To fix: Create the level file or update load_level in your HEIROC file.\\n\");\n");
            } else {
                output.push_str(&format!("    // Load level: {}\n", load_level));
                output.push_str(&format!("    heidic_load_level(\"{}\");\n", load_level));
            }
            output.push_str("\n");
        }
        
        output.push_str("    print(\"Starting render loop...\\n\");\n");
        output.push_str("    print(\"Press ESC or close the window to exit.\\n\");\n");
        output.push_str("\n");
        
        let target_frame_time = 1.0 / (fps_max as f64);
        output.push_str(&format!("    let target_frame_time: f32 = {};  // {} FPS cap\n", target_frame_time, fps_max));
        output.push_str("\n");
        output.push_str("    while glfwWindowShouldClose(window) == 0 {\n");
        output.push_str("        let frame_start: f32 = glfwGetTime();\n");
        output.push_str("        glfwPollEvents();\n");
        output.push_str("\n");
        output.push_str("        if glfwGetKey(window, 256) == 1 { // ESC key\n");
        output.push_str("            glfwSetWindowShouldClose(window, 1);\n");
        output.push_str("        }\n");
        output.push_str("\n");
        
        // Only generate NEUROSHELL update code if enabled
        if neuroshell_enabled {
            output.push_str("        // Update NEUROSHELL (if enabled)\n");
            output.push_str("        if (neuroshell_is_enabled()) {\n");
            output.push_str("            let delta_time: f32 = 0.016;  // ~60 FPS\n");
            output.push_str("            neuroshell_update(delta_time);\n");
            output.push_str("        }\n");
            output.push_str("\n");
        }
        output.push_str("        heidic_render_frame(window);\n");
        output.push_str("\n");
        output.push_str("        // FPS cap\n");
        output.push_str("        let frame_time: f32 = glfwGetTime() - frame_start;\n");
        output.push_str("        if frame_time < target_frame_time {\n");
        output.push_str("            let sleep_ms: i32 = f32_to_i32((target_frame_time - frame_time) * 1000);\n");
        output.push_str("            heidic_sleep_ms(sleep_ms);\n");
        output.push_str("        }\n");
        output.push_str("    }\n");
        output.push_str("\n");
        output.push_str("    print(\"Cleaning up...\\n\");\n");
        
        // Only generate NEUROSHELL cleanup code if enabled
        if neuroshell_enabled {
            output.push_str("    // Cleanup NEUROSHELL (if enabled)\n");
            output.push_str("    if (neuroshell_is_enabled()) {\n");
            output.push_str("        neuroshell_shutdown();\n");
            output.push_str("    }\n");
            output.push_str("\n");
        }
        output.push_str("    heidic_cleanup_renderer();\n");
        output.push_str("    glfwDestroyWindow(window);\n");
        output.push_str("    glfwTerminate();\n");
        output.push_str("    print(\"Program exited successfully.\\n\");\n");
        output.push_str("    print(\"Done!\\n\");\n");
        output.push_str("}\n");
        
        Ok(output)
    }
}

