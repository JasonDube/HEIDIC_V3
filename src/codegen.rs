use crate::ast::*;
use anyhow::Result;
use std::collections::HashMap;

pub struct CodeGenerator {
    components: HashMap<String, ComponentDef>,  // Store component metadata for SOA detection
    hot_systems: Vec<SystemDef>,  // Store hot-reloadable systems
    hot_shaders: Vec<ShaderDef>,  // Store hot-reloadable shaders
    hot_components: Vec<ComponentDef>,  // Store hot-reloadable components
    has_resources: bool,  // Track if program has resource declarations
}

impl CodeGenerator {
    pub fn new() -> Self {
        Self {
            components: HashMap::new(),
            hot_systems: Vec::new(),
            hot_shaders: Vec::new(),
            hot_components: Vec::new(),
            has_resources: false,
        }
    }
    
    pub fn generate(&mut self, program: &Program) -> Result<String> {
        let mut output = String::new();
        
        // First pass: collect component metadata (for SOA detection), hot systems, hot shaders, and hot components
        for item in &program.items {
            if let Item::Component(c) = item {
                self.components.insert(c.name.clone(), c.clone());
                if c.is_hot {
                    self.hot_components.push(c.clone());
                }
            }
            if let Item::System(s) = item {
                if s.is_hot {
                    self.hot_systems.push(s.clone());
                }
            }
            if let Item::Shader(sh) = item {
                if sh.is_hot {
                    self.hot_shaders.push(sh.clone());
                }
            }
        }
        
        // Generate includes and standard library (AFTER collecting hot items so we know what to include)
        output.push_str("#include <iostream>\n");
        output.push_str("#include <vector>\n");
        output.push_str("#include <string>\n");
        output.push_str("#include <unordered_map>\n");
        output.push_str("#include <memory>\n");
        output.push_str("#include <cmath>\n");
        output.push_str("#include <cstdint>\n");
        // Include chrono if we have hot components (for ECS timing) or hot systems/shaders
        if !self.hot_components.is_empty() || !self.hot_systems.is_empty() || !self.hot_shaders.is_empty() {
            output.push_str("#include <chrono>\n");
        }
        output.push_str("\n");
        
        // Include EDEN standard library (Vulkan, GLFW, GLM math, ImGui)
        output.push_str("// EDEN ENGINE Standard Library\n");
        output.push_str("#include \"stdlib/vulkan.h\"\n");
        output.push_str("#include \"stdlib/glfw.h\"\n");
        output.push_str("#include \"stdlib/math.h\"\n");
        output.push_str("#include \"stdlib/imgui.h\"\n");
        // Include entity storage if we have hot components
        if !self.hot_components.is_empty() {
            output.push_str("#include \"stdlib/entity_storage.h\"\n");
        }
        output.push_str("\n");
        
        // Generate structs and components
        for item in &program.items {
            match item {
                Item::Struct(s) => {
                    output.push_str(&self.generate_struct(s, 0));
                }
                Item::Component(c) => {
                    output.push_str(&self.generate_component(c, 0));
                }
                _ => {}
            }
        }
        
        // Generate resources (need to include resource.h header)
        // Check if we have any resources (for includes) and @hot resources (for hot-reload)
        let mut has_any_resources = false;
        self.has_resources = false;
        for item in &program.items {
            if let Item::Resource(res) = item {
                has_any_resources = true;
                if res.is_hot {
                    self.has_resources = true;
                }
            }
        }
        if has_any_resources {
            output.push_str("#include \"stdlib/resource.h\"\n");
            output.push_str("#include \"stdlib/texture_resource.h\"\n");
            output.push_str("#include \"stdlib/mesh_resource.h\"\n");
            output.push_str("\n");
        }
        
        // Generate resource declarations
        for item in &program.items {
            if let Item::Resource(res) = item {
                output.push_str(&self.generate_resource(res));
            }
        }
        
        // Generate resource accessor functions (so resources can be accessed in HEIDIC)
        if self.has_resources {
            output.push_str("\n// Resource accessor functions (for HEIDIC access)\n");
            for item in &program.items {
                if let Item::Resource(res) = item {
                    output.push_str(&self.generate_resource_accessor(res));
                }
            }
            output.push_str("\n");
        }
        
        // Generate extern function declarations (C linkage)
        // Note: Resource accessor functions are already implemented above, so we don't need to declare them here
        let mut extern_libraries = std::collections::HashSet::new();
        
        for item in &program.items {
            if let Item::ExternFunction(ext) = item {
                output.push_str("extern \"C\" {\n");
                // Special case: heidic_render_balls needs positions/sizes arrays when using ECS
                if ext.name == "heidic_render_balls" && !self.hot_components.is_empty() {
                    output.push_str(&format!("    void heidic_render_balls(GLFWwindow* window, int32_t ball_count, float* positions, float* sizes);\n"));
                } else {
                    let return_type = self.type_to_cpp_for_extern(&ext.return_type);
                    output.push_str(&format!("    {} {}(", return_type, ext.name));
                    for (i, param) in ext.params.iter().enumerate() {
                        if i > 0 {
                            output.push_str(", ");
                        }
                        // For extern C functions, convert string to const char*
                        let param_type = if matches!(param.ty, Type::String) {
                            "const char*".to_string()
                        } else {
                            self.type_to_cpp_for_extern(&param.ty)
                        };
                        output.push_str(&format!("{} {}", param_type, param.name));
                    }
                    output.push_str(");\n");
                }
                output.push_str("}\n");
                
                if let Some(ref lib) = ext.library {
                    extern_libraries.insert(lib.clone());
                }
            }
        }
        if !extern_libraries.is_empty() {
            output.push_str("\n// Link libraries: ");
            for lib in &extern_libraries {
                output.push_str(lib);
                output.push_str(" ");
            }
            output.push_str("\n");
        }
        output.push_str("\n");
        
        // Generate forward declarations for all functions
        let mut functions = Vec::new();
        let mut has_main = false;
        for item in &program.items {
            match item {
                Item::Function(f) => {
                    if f.name == "main" {
                        has_main = true;
                    }
                    functions.push(f.clone());
                    // Generate forward declaration
                    let func_name = if f.name == "main" {
                        "heidic_main".to_string()
                    } else {
                        f.name.clone()
                    };
                    let return_type = if f.name == "main" && matches!(f.return_type, Type::Void) {
                        "int".to_string()
                    } else {
                        self.type_to_cpp(&f.return_type)
                    };
                    output.push_str(&format!("{} {}(", return_type, func_name));
                    for (i, param) in f.params.iter().enumerate() {
                        if i > 0 {
                            output.push_str(", ");
                        }
                        output.push_str(&format!("{} {}", 
                            self.type_to_cpp(&param.ty), 
                            param.name));
                    }
                    output.push_str(");\n");
                }
                Item::System(s) => {
                    // Only generate forward declarations for non-hot systems
                    // Hot systems are in separate DLLs
                    if !s.is_hot {
                        for func in &s.functions {
                            functions.push(func.clone());
                            // Generate forward declaration
                            output.push_str(&format!("{} {}(", 
                                self.type_to_cpp(&func.return_type), 
                                func.name));
                            for (i, param) in func.params.iter().enumerate() {
                                if i > 0 {
                                    output.push_str(", ");
                                }
                                output.push_str(&format!("{} {}", 
                                    self.type_to_cpp(&param.ty), 
                                    param.name));
                            }
                            output.push_str(");\n");
                        }
                    } else {
                        // Generate function pointer declarations for hot systems
                        for func in &s.functions {
                            let return_type = self.type_to_cpp(&func.return_type);
                            // Generate function pointer type
                            output.push_str(&format!("// Hot-reloadable function: {}\n", func.name));
                            output.push_str(&format!("typedef {} (*{}_ptr)(", return_type, func.name));
                            for (i, param) in func.params.iter().enumerate() {
                                if i > 0 {
                                    output.push_str(", ");
                                }
                                output.push_str(&format!("{}", self.type_to_cpp(&param.ty)));
                            }
                            output.push_str(");\n");
                            output.push_str(&format!("extern {}_ptr g_{};\n\n", func.name, func.name));
                        }
                    }
                }
                _ => {}
            }
        }
        output.push_str("\n");
        
        // Generate forward declarations for hot-reload functions if we have hot systems
        if !self.hot_systems.is_empty() {
            output.push_str("// Hot-reload function forward declarations\n");
            output.push_str("void check_and_reload_hot_system();\n");
            output.push_str("void load_hot_system(const char* dll_path);\n");
            output.push_str("void unload_hot_system();\n");
            output.push_str("\n");
        }
        
        // Generate forward declarations for shader hot-reload functions if we have hot shaders
        if !self.hot_shaders.is_empty() {
            output.push_str("// Shader hot-reload function forward declarations\n");
            output.push_str("void check_and_reload_hot_shaders();\n");
            output.push_str("extern \"C\" void heidic_reload_shader(const char* shader_path);\n");
            output.push_str("\n");
        }
        
        // Generate forward declarations for resource hot-reload functions if we have resources
        if self.has_resources {
            output.push_str("// Resource hot-reload function forward declarations\n");
            output.push_str("void check_and_reload_resources();\n");
            output.push_str("\n");
        }
        
        // Generate forward declarations for component hot-reload functions if we have hot components
        if !self.hot_components.is_empty() {
            output.push_str("// Component hot-reload function forward declarations\n");
            output.push_str("void check_and_migrate_hot_components();\n");
            output.push_str("void init_component_versions();\n");
            output.push_str("\n");
            
            // Generate ECS storage globals
            output.push_str("// ECS storage for hot components\n");
            output.push_str("static EntityStorage g_storage;\n");
            output.push_str("static std::vector<EntityId> g_entities;\n");
            output.push_str("static constexpr float BOUNDS = 3.0f;\n");
            output.push_str("static auto g_last_update_time = std::chrono::high_resolution_clock::now();\n");
            output.push_str("\n");
        }
        
        // Generate function implementations (excluding hot systems)
        for f in &functions {
            // Check if this function is from a hot system
            let is_hot = self.hot_systems.iter().any(|s| {
                s.functions.iter().any(|sf| sf.name == f.name)
            });
            if !is_hot {
                output.push_str(&self.generate_function(f, 0));
            }
        }
        
        // Generate hot-reload runtime integration
        if !self.hot_systems.is_empty() {
            output.push_str("\n// Hot-Reload Runtime Integration\n");
            output.push_str("#include <windows.h>\n");
            output.push_str("#include <string>\n");
            output.push_str("#include <thread>\n");
            output.push_str("#include <chrono>\n");
            output.push_str("\n");
            
            // Generate function pointer variables
            for system in &self.hot_systems {
                for func in &system.functions {
                    output.push_str(&format!("{}_ptr g_{} = nullptr;\n", func.name, func.name));
                }
            }
            
            output.push_str("\n");
            output.push_str("// Hot-reload helper functions\n");
            output.push_str("HMODULE g_hot_dll = nullptr;\n");
            output.push_str("\n");
            output.push_str("void load_hot_system(const char* dll_path) {\n");
            output.push_str("    // Unload old DLL if loaded\n");
            output.push_str("    if (g_hot_dll) {\n");
            output.push_str("        FreeLibrary(g_hot_dll);\n");
            output.push_str("        g_hot_dll = nullptr;\n");
            output.push_str("    }\n");
            output.push_str("    \n");
            output.push_str("    // Load new DLL\n");
            output.push_str("    g_hot_dll = LoadLibraryA(dll_path);\n");
            output.push_str("    if (!g_hot_dll) {\n");
            output.push_str("        std::cerr << \"Failed to load hot-reload DLL: \" << dll_path << std::endl;\n");
            output.push_str("        return;\n");
            output.push_str("    }\n");
            output.push_str("    \n");
            output.push_str("    // Load function pointers\n");
            for system in &self.hot_systems {
                for func in &system.functions {
                    output.push_str(&format!("    g_{} = ({}_ptr)GetProcAddress(g_hot_dll, \"{}\");\n", 
                        func.name, func.name, func.name));
                    output.push_str(&format!("    if (!g_{}) {{\n", func.name));
                    output.push_str(&format!("        std::cerr << \"Failed to load function: {}\" << std::endl;\n", func.name));
                    output.push_str("    }\n");
                }
            }
            output.push_str("}\n");
            output.push_str("\n");
            output.push_str("void unload_hot_system() {\n");
            output.push_str("    if (g_hot_dll) {\n");
            output.push_str("        FreeLibrary(g_hot_dll);\n");
            output.push_str("        g_hot_dll = nullptr;\n");
            for system in &self.hot_systems {
                for func in &system.functions {
                    output.push_str(&format!("        g_{} = nullptr;\n", func.name));
                }
            }
            output.push_str("    }\n");
            output.push_str("}\n");
            output.push_str("\n");
            output.push_str("// File watching and auto-reload\n");
            output.push_str("#include <sys/stat.h>\n");
            output.push_str("#include <io.h>\n");
            output.push_str("#include <chrono>\n");
            output.push_str("\n");
            output.push_str("static time_t g_last_dll_time = 0;\n");
            output.push_str("static std::chrono::steady_clock::time_point g_startup_time = std::chrono::steady_clock::now();\n");
            output.push_str("static const int STARTUP_GRACE_PERIOD_SECONDS = 3; // Ignore DLL changes for first 3 seconds after startup\n");
            output.push_str("\n");
            output.push_str("void check_and_reload_hot_system() {\n");
            output.push_str("    // Ignore DLL changes during startup grace period (to avoid reloading immediately after build)\n");
            output.push_str("    auto now = std::chrono::steady_clock::now();\n");
            output.push_str("    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - g_startup_time).count();\n");
            output.push_str("    if (elapsed < STARTUP_GRACE_PERIOD_SECONDS) {\n");
            output.push_str("        return; // Still in startup grace period\n");
            output.push_str("    }\n");
            for system in &self.hot_systems {
                let dll_name = format!("{}.dll", system.name.to_lowercase());
                output.push_str(&format!("    // Check {} DLL file modification time\n", system.name));
                output.push_str(&format!("    struct stat dll_stat;\n"));
                output.push_str(&format!("    if (stat(\"{}\", &dll_stat) == 0) {{\n", dll_name));
                output.push_str(&format!("        if (dll_stat.st_mtime > g_last_dll_time) {{\n"));
                output.push_str(&format!("            g_last_dll_time = dll_stat.st_mtime;\n"));
                output.push_str(&format!("            std::cout << \"[Hot-Reload] Detected change in {}, reloading...\" << std::endl;\n", dll_name));
                output.push_str(&format!("            // Unload old DLL first\n"));
                output.push_str(&format!("            unload_hot_system();\n"));
                output.push_str(&format!("            // Small delay to ensure DLL is fully unloaded on Windows\n"));
                output.push_str(&format!("            std::this_thread::sleep_for(std::chrono::milliseconds(100));\n"));
                output.push_str(&format!("            load_hot_system(\"{}\");\n", dll_name));
                output.push_str(&format!("            std::cout << \"[Hot-Reload] {} reloaded successfully!\" << std::endl;\n", system.name));
                output.push_str(&format!("        }}\n"));
                output.push_str(&format!("    }}\n"));
            }
            output.push_str("}\n");
            output.push_str("\n");
        }
        
        // Generate shader hot-reload runtime integration
        if !self.hot_shaders.is_empty() {
            output.push_str("\n// Shader Hot-Reload Runtime Integration\n");
            output.push_str("#include <sys/stat.h>\n");
            output.push_str("#include <io.h>\n");
            output.push_str("#include <map>\n");
            output.push_str("#include <string>\n");
            output.push_str("\n");
            output.push_str("// Store last modification times for hot shaders\n");
            output.push_str("static std::map<std::string, time_t> g_shader_mtimes;\n");
            output.push_str("\n");
            output.push_str("void check_and_reload_hot_shaders() {\n");
            for (idx, shader) in self.hot_shaders.iter().enumerate() {
                // Get the shader file path (could be .glsl or .spv)
                let shader_path = &shader.path;
                // Determine the .spv path - keep extension to avoid conflicts (e.g., my_shader.vert.spv)
                let spv_path = if shader_path.ends_with(".glsl") {
                    shader_path.replace(".glsl", ".spv")
                } else if shader_path.ends_with(".vert") || shader_path.ends_with(".frag") || shader_path.ends_with(".comp") {
                    format!("{}.spv", shader_path)  // my_shader.vert.spv, my_shader.frag.spv
                } else {
                    format!("{}.spv", shader_path)
                };
                
                // Use unique variable name for each shader
                let stat_var_name = format!("shader_stat_{}", idx);
                
                output.push_str(&format!("    // Check {} shader file modification time\n", shader_path));
                output.push_str(&format!("    struct stat {};\n", stat_var_name));
                output.push_str(&format!("    if (stat(\"{}\", &{}) == 0) {{\n", spv_path, stat_var_name));
                output.push_str(&format!("        auto it = g_shader_mtimes.find(\"{}\");\n", spv_path));
                output.push_str(&format!("        time_t last_mtime = (it != g_shader_mtimes.end()) ? it->second : 0;\n"));
                output.push_str(&format!("        if ({}.st_mtime > last_mtime) {{\n", stat_var_name));
                output.push_str(&format!("            g_shader_mtimes[\"{}\"] = {}.st_mtime;\n", spv_path, stat_var_name));
                output.push_str(&format!("            std::cout << \"[Shader Hot-Reload] Detected change in {}, reloading...\" << std::endl;\n", spv_path));
                // Pass the original source path so we can determine shader stage (vertex/fragment)
                output.push_str(&format!("            heidic_reload_shader(\"{}\");\n", shader_path));
                output.push_str(&format!("            std::cout << \"[Shader Hot-Reload] {} reloaded successfully!\" << std::endl;\n", spv_path));
                output.push_str(&format!("        }}\n"));
                output.push_str(&format!("    }}\n"));
            }
            output.push_str("}\n");
            output.push_str("\n");
            // Initialize shader modification times at startup
            output.push_str("static void init_shader_mtimes() {\n");
            for (idx, shader) in self.hot_shaders.iter().enumerate() {
                let shader_path = &shader.path;
                // Use same naming as check_and_reload_hot_shaders: keep extension for .vert/.frag/.comp
                let spv_path = if shader_path.ends_with(".glsl") {
                    shader_path.replace(".glsl", ".spv")
                } else if shader_path.ends_with(".vert") || shader_path.ends_with(".frag") || shader_path.ends_with(".comp") {
                    format!("{}.spv", shader_path)  // my_shader.vert.spv, my_shader.frag.spv
                } else {
                    format!("{}.spv", shader_path)
                };
                // Use unique variable name for each shader
                let stat_var_name = format!("shader_stat_init_{}", idx);
                output.push_str(&format!("    struct stat {};\n", stat_var_name));
                output.push_str(&format!("    if (stat(\"{}\", &{}) == 0) {{\n", spv_path, stat_var_name));
                output.push_str(&format!("        g_shader_mtimes[\"{}\"] = {}.st_mtime;\n", spv_path, stat_var_name));
                output.push_str(&format!("    }}\n"));
            }
            output.push_str("}\n");
            output.push_str("\n");
        }
        
        // Generate resource hot-reload runtime integration
        if self.has_resources {
            output.push_str("\n// Resource Hot-Reload Runtime Integration (CONTINUUM)\n");
            output.push_str("void check_and_reload_resources() {\n");
            for item in &program.items {
                if let Item::Resource(res) = item {
                    let global_name = format!("g_resource_{}", res.name.to_lowercase());
                    output.push_str(&format!("    // Check {} resource file modification time\n", res.name));
                    output.push_str(&format!("    if ({}.reload()) {{\n", global_name));
                    output.push_str(&format!("        std::cout << \"[Resource Hot-Reload] {} reloaded successfully!\" << std::endl;\n", res.name));
                    output.push_str(&format!("    }}\n"));
                }
            }
            output.push_str("}\n");
            output.push_str("\n");
        }
        
        // Generate component hot-reload runtime integration
        if !self.hot_components.is_empty() {
            output.push_str("\n// Component Hot-Reload Runtime Integration\n");
            output.push_str("#include <sys/stat.h>\n");
            output.push_str("#include <io.h>\n");
            output.push_str("#include <map>\n");
            output.push_str("#include <string>\n");
            output.push_str("#include <cstring>\n");
            output.push_str("#include <cstdio>\n");
            output.push_str("\n");
            
            // Generate component metadata structs
            output.push_str("// Component metadata for version tracking\n");
            output.push_str("struct ComponentMetadata {\n");
            output.push_str("    const char* name;\n");
            output.push_str("    uint32_t version;\n");
            output.push_str("    size_t size;\n");
            output.push_str("    const char* field_signature;  // Hash of field names and types\n");
            output.push_str("};\n");
            output.push_str("\n");
            output.push_str("// Storage for loaded field signatures (since we can't store const char* directly)\n");
            for component in &self.hot_components {
                let comp_name_lower = component.name.to_lowercase();
                output.push_str(&format!("static char g_prev_sig_storage_{}[512] = {{0}};\n", comp_name_lower));
            }
            output.push_str("\n");
            
            // Generate metadata for each hot component
            for component in &self.hot_components {
                // Generate field signature (hash of field names and types)
                let mut field_sig = String::new();
                for field in &component.fields {
                    field_sig.push_str(&field.name);
                    field_sig.push(':');
                    field_sig.push_str(&self.type_to_cpp(&field.ty));
                    field_sig.push(';');
                }
                
                // Generate component version (starts at 1, will increment on layout changes)
                output.push_str(&format!("// Metadata for component: {}\n", component.name));
                output.push_str(&format!("static ComponentMetadata g_metadata_{} = {{\n", component.name.to_lowercase()));
                output.push_str(&format!("    \"{}\",\n", component.name));
                output.push_str(&format!("    1,  // Version (increments when layout changes)\n"));
                output.push_str(&format!("    sizeof({}),\n", component.name));
                output.push_str(&format!("    \"{}\"  // Field signature\n", field_sig));
                output.push_str("};\n");
                output.push_str("\n");
                
                // Generate previous version metadata (for migration detection)
                // This will be updated when the layout changes
                output.push_str(&format!("// Previous version metadata (for migration)\n"));
                output.push_str(&format!("static ComponentMetadata g_prev_metadata_{} = {{\n", component.name.to_lowercase()));
                output.push_str(&format!("    \"{}\",\n", component.name));
                output.push_str(&format!("    0,  // Previous version (0 = no previous version)\n"));
                output.push_str(&format!("    0,  // Previous size\n"));
                output.push_str(&format!("    \"\"  // Previous field signature\n"));
                output.push_str("};\n");
                output.push_str("\n");
            }
            
            // Generate version tracking map
            output.push_str("// Track component versions at runtime\n");
            output.push_str("static std::map<std::string, uint32_t> g_component_versions;\n");
            output.push_str("static std::map<std::string, time_t> g_component_file_mtimes;\n");
            output.push_str("\n");
            
            // Generate migration functions for each component
            // These functions migrate from previous version to current version
            for component in &self.hot_components {
                self.generate_migration_function(&mut output, component);
            }
            
            // Generate initialization function
            output.push_str("void init_component_versions() {\n");
            output.push_str("    // Load previous component metadata from text file (if it exists)\n");
            output.push_str("    // Format: ComponentName:Version:FieldSignature (one per line)\n");
            output.push_str("    FILE* meta_file = fopen(\".heidic_component_versions.txt\", \"r\");\n");
            output.push_str("    if (meta_file) {\n");
            output.push_str("        char line[1024];\n");
            output.push_str("        while (fgets(line, sizeof(line), meta_file)) {\n");
            output.push_str("            char name[256], sig[512];\n");
            output.push_str("            uint32_t version;\n");
            output.push_str("            if (sscanf(line, \"%255[^:]:%u:%511[^\\n]\", name, &version, sig) == 3) {\n");
            for component in &self.hot_components {
                let comp_name_lower = component.name.to_lowercase();
                output.push_str(&format!("                if (strcmp(name, \"{}\") == 0) {{\n", component.name));
                output.push_str(&format!("                    g_prev_metadata_{}.name = \"{}\";\n", comp_name_lower, component.name));
                output.push_str(&format!("                    g_prev_metadata_{}.version = version;\n", comp_name_lower));
                output.push_str(&format!("                    strncpy(g_prev_sig_storage_{}, sig, sizeof(g_prev_sig_storage_{}) - 1);\n", comp_name_lower, comp_name_lower));
                output.push_str(&format!("                    g_prev_metadata_{}.field_signature = g_prev_sig_storage_{};\n", comp_name_lower, comp_name_lower));
                output.push_str(&format!("                }}\n"));
            }
            output.push_str("            }\n");
            output.push_str("        }\n");
            output.push_str("        fclose(meta_file);\n");
            output.push_str("    } else {\n");
            output.push_str("        // First run: initialize previous metadata to current\n");
            for component in &self.hot_components {
                let comp_name_lower = component.name.to_lowercase();
                output.push_str(&format!("        g_prev_metadata_{} = g_metadata_{};\n", comp_name_lower, comp_name_lower));
            }
            output.push_str("    }\n");
            output.push_str("    \n");
            output.push_str("    // Initialize version tracking\n");
            for component in &self.hot_components {
                let comp_name_lower = component.name.to_lowercase();
                output.push_str(&format!("    g_component_versions[\"{}\"] = g_metadata_{}.version;\n", 
                    component.name, comp_name_lower));
            }
            output.push_str("    \n");
            output.push_str("    // Save current metadata to file for next run\n");
            output.push_str("    meta_file = fopen(\".heidic_component_versions.txt\", \"w\");\n");
            output.push_str("    if (meta_file) {\n");
            for component in &self.hot_components {
                let comp_name_lower = component.name.to_lowercase();
                output.push_str(&format!("        fprintf(meta_file, \"{}:%u:%s\\n\", g_metadata_{}.version, g_metadata_{}.field_signature);\n", 
                    component.name, comp_name_lower, comp_name_lower));
            }
            output.push_str("        fclose(meta_file);\n");
            output.push_str("    }\n");
            output.push_str("}\n");
            output.push_str("\n");
            
            // Generate component layout change detection and migration
            output.push_str("void check_and_migrate_hot_components() {\n");
            output.push_str("    // Check each hot component for layout changes\n");
            for component in &self.hot_components {
                let comp_name_lower = component.name.to_lowercase();
                output.push_str(&format!("    // Check component: {}\n", component.name));
                output.push_str(&format!("    if (g_prev_metadata_{}.version > 0 && ", comp_name_lower));
                output.push_str(&format!("strcmp(g_metadata_{}.field_signature, g_prev_metadata_{}.field_signature) != 0) {{\n", 
                    comp_name_lower, comp_name_lower));
                output.push_str(&format!("        std::cout << \"[Component Hot-Reload] Detected layout change in {}, migrating entities...\" << std::endl;\n", 
                    component.name));
                output.push_str(&format!("        migrate_{}(g_prev_metadata_{}.version, g_metadata_{}.version);\n", 
                    comp_name_lower, comp_name_lower, comp_name_lower));
                output.push_str(&format!("        // Update previous metadata to current\n"));
                output.push_str(&format!("        g_prev_metadata_{} = g_metadata_{};\n", comp_name_lower, comp_name_lower));
                output.push_str(&format!("        g_component_versions[\"{}\"] = g_metadata_{}.version;\n", 
                    component.name, comp_name_lower));
                output.push_str(&format!("        // Save updated metadata to file\n"));
                output.push_str(&format!("        FILE* meta_file = fopen(\".heidic_component_versions.txt\", \"w\");\n"));
                output.push_str(&format!("        if (meta_file) {{\n"));
                for comp in &self.hot_components {
                    let comp_lower = comp.name.to_lowercase();
                    output.push_str(&format!("            fprintf(meta_file, \"{}:%u:%s\\n\", g_metadata_{}.version, g_metadata_{}.field_signature);\n", 
                        comp.name, comp_lower, comp_lower));
                }
                output.push_str(&format!("            fclose(meta_file);\n"));
                output.push_str(&format!("        }}\n"));
                output.push_str(&format!("        std::cout << \"[Component Hot-Reload] {} migration complete!\" << std::endl;\n", 
                    component.name));
                output.push_str("    }\n");
            }
            output.push_str("}\n");
            output.push_str("\n");
        }
        
        // Add C++ main wrapper if HEIDIC main exists
        if has_main {
            output.push_str("int main(int argc, char* argv[]) {\n");
            // Load hot-reloadable systems at startup
            if !self.hot_systems.is_empty() {
                for system in &self.hot_systems {
                    let dll_name = format!("{}.dll", system.name.to_lowercase());
                    let dll_cpp_name = format!("{}_hot.dll.cpp", system.name.to_lowercase());
                    output.push_str(&format!("    // Initialize file watching\n"));
                    output.push_str(&format!("    struct stat dll_stat;\n"));
                    output.push_str(&format!("    if (stat(\"{}\", &dll_stat) == 0) {{\n", dll_cpp_name));
                    output.push_str(&format!("        g_last_dll_time = dll_stat.st_mtime;\n"));
                    output.push_str(&format!("    }}\n"));
                    output.push_str(&format!("    load_hot_system(\"{}\");\n", dll_name));
                }
            }
            // Initialize shader modification times at startup
            if !self.hot_shaders.is_empty() {
                output.push_str("    init_shader_mtimes();\n");
            }
            // Initialize component versions at startup
            if !self.hot_components.is_empty() {
                output.push_str("    init_component_versions();\n");
            }
            output.push_str("    heidic_main();\n");
            // Only unload hot system if we have hot systems
            if !self.hot_systems.is_empty() {
                output.push_str("    unload_hot_system();\n");
            }
            output.push_str("    return 0;\n");
            output.push_str("}\n");
        }
        
        Ok(output)
    }
    
    // Generate DLL source file for a hot system
    pub fn generate_hot_system_dll(&self, system: &SystemDef) -> String {
        let mut output = String::new();
        
        output.push_str("// Hot-reloadable system DLL\n");
        output.push_str("// Auto-generated from @hot system\n");
        output.push_str("#include <cmath>\n");
        output.push_str("#include <cstdint>\n");
        output.push_str("\n");
        
        // Generate function implementations with extern "C"
        for func in &system.functions {
            output.push_str("extern \"C\" {\n");
            let return_type = self.type_to_cpp(&func.return_type);
            output.push_str(&format!("    {} {}(", return_type, func.name));
            for (i, param) in func.params.iter().enumerate() {
                if i > 0 {
                    output.push_str(", ");
                }
                output.push_str(&format!("{} {}", 
                    self.type_to_cpp(&param.ty), 
                    param.name));
            }
            output.push_str(") {\n");
            
            // Generate function body (statements)
            for stmt in &func.body {
                output.push_str(&self.generate_statement(stmt, 2));
            }
            
            // Add default return if function has return type but no return statement
            if !matches!(func.return_type, Type::Void) {
                // Check if last statement is a return
                let has_return = func.body.iter().any(|s| matches!(s, Statement::Return(_)));
                if !has_return {
                    // Generate default return value based on type
                    let default_value = match func.return_type {
                        Type::I32 | Type::I64 => "0",
                        Type::F32 | Type::F64 => "0.0f",
                        Type::Bool => "false",
                        Type::String => "\"\"",
                        _ => "{}",
                    };
                    output.push_str(&format!("        return {};\n", default_value));
                }
            }
            
            output.push_str("    }\n");
            output.push_str("}\n");
            output.push_str("\n");
        }
        
        output
    }
    
    // Get list of hot systems (for generating DLL files)
    pub fn get_hot_systems(&self) -> &Vec<SystemDef> {
        &self.hot_systems
    }
    
    // Generate migration function for a component
    fn generate_migration_function(&self, output: &mut String, component: &ComponentDef) {
        let comp_name_lower = component.name.to_lowercase();
        
        // Migration function signature
        output.push_str(&format!("// Migration function for component: {}\n", component.name));
        output.push_str(&format!("// Migrates entity data from old version to new version\n"));
        output.push_str(&format!("void migrate_{}(uint32_t old_version, uint32_t new_version) {{\n", comp_name_lower));
        output.push_str(&format!("    std::cout << \"[Component Migration] {}: v\" << old_version << \" -> v\" << new_version << std::endl;\n", 
            component.name));
        
        if component.fields.is_empty() {
            output.push_str("    // No fields to migrate (empty component)\n");
            output.push_str("    return;\n");
            output.push_str("}\n");
            return;
        }
        
        // Parse old field signature to determine which fields existed in old version
        output.push_str(&format!("    // Parse old field signature to determine which fields existed in old version\n"));
        output.push_str(&format!("    std::string old_sig = g_prev_metadata_{}.field_signature;\n", comp_name_lower));
        
        // Generate field existence checks for each field
        // Field signature format: "field_name:type;"
        for field in &component.fields {
            let type_name = self.type_to_cpp(&field.ty);
            output.push_str(&format!("    bool has_{}_in_old = old_sig.find(\"{}:{}\") != std::string::npos;\n", 
                field.name, field.name, type_name));
        }
        output.push_str("\n");
        
        // Collect all entities with this component first (to avoid iterator invalidation)
        output.push_str(&format!("    // Collect all entities with {} component (to avoid iterator invalidation during migration)\n", component.name));
        output.push_str("    std::vector<EntityId> entities_to_migrate;\n");
        output.push_str(&format!("    g_storage.for_each<{}>([&](EntityId e, {}&) {{\n", component.name, component.name));
        output.push_str("        entities_to_migrate.push_back(e);\n");
        output.push_str(&format!("    }});\n"));
        output.push_str("\n");
        output.push_str("    // Migrate each entity\n");
        output.push_str("    int migrated_count = 0;\n");
        output.push_str("    for (EntityId e : entities_to_migrate) {\n");
        output.push_str(&format!("        {}* old_comp_ptr = g_storage.get_component<{}>(e);\n", component.name, component.name));
        output.push_str("        if (!old_comp_ptr) continue;  // Entity no longer has component\n");
        output.push_str(&format!("        {} old_comp = *old_comp_ptr;  // Copy old data before removing\n", component.name));
        output.push_str("\n");
        output.push_str(&format!("        // Create new component instance, zero-initialized\n"));
        output.push_str(&format!("        {} new_comp{{}};\n", component.name));
        output.push_str("\n");
        
        // Copy fields that existed in old version, use defaults for new fields
        output.push_str("        // Copy fields that existed in old version\n");
        for field in &component.fields {
            let default_val = self.get_default_value_for_type(&field.ty);
            output.push_str(&format!("        if (has_{}_in_old) {{\n", field.name));
            output.push_str(&format!("            new_comp.{} = old_comp.{};  // Copy existing field\n", field.name, field.name));
            output.push_str(&format!("        }} else {{\n"));
            output.push_str(&format!("            new_comp.{} = {};  // New field, use default\n", field.name, default_val));
            output.push_str(&format!("        }}\n"));
        }
        
        output.push_str("\n");
        output.push_str("        // Replace old component with new one\n");
        output.push_str(&format!("        g_storage.remove_component<{}>(e);\n", component.name));
        output.push_str(&format!("        g_storage.add_component<{}>(e, new_comp);\n", component.name));
        output.push_str("        migrated_count++;\n");
        output.push_str("    }\n");
        
        output.push_str("\n");
        output.push_str(&format!("    std::cout << \"[Component Migration] Migrated \" << migrated_count << \" {} entities\" << std::endl;\n", 
            component.name));
        output.push_str("}\n");
        output.push_str("\n");
    }
    
    // Get default value for a type (for new fields in migrations)
    fn get_default_value_for_type(&self, ty: &Type) -> String {
        match ty {
            Type::I32 | Type::I64 => "0",
            Type::F32 | Type::F64 => "0.0f",
            Type::Bool => "false",
            Type::String => "\"\"",
            Type::Vec2 => "Vec2(0.0f, 0.0f)",
            Type::Vec3 => "Vec3(0.0f, 0.0f, 0.0f)",
            Type::Vec4 => "Vec4(0.0f, 0.0f, 0.0f, 1.0f)",
            Type::Mat4 => "Mat4(1.0f)", // Identity matrix
            Type::Array(_) => "{}", // Empty array
            _ => "{}", // Default initialization
        }.to_string()
    }
    
    fn generate_struct(&self, s: &StructDef, indent: usize) -> String {
        let mut output = format!("struct {} {{\n", s.name);
        for field in &s.fields {
            output.push_str(&format!("{}    {} {};\n", 
                self.indent(indent + 1), 
                self.type_to_cpp(&field.ty), 
                field.name));
        }
        output.push_str("};\n\n");
        output
    }
    
    fn generate_component(&self, c: &ComponentDef, indent: usize) -> String {
        let mut output = format!("struct {} {{\n", c.name);
        for field in &c.fields {
            output.push_str(&format!("{}    {} {};\n", 
                self.indent(indent + 1), 
                self.type_to_cpp(&field.ty), 
                field.name));
        }
        output.push_str("};\n\n");
        output
    }
    
    fn generate_resource(&self, res: &ResourceDef) -> String {
        // Map resource type to C++ class name
        let cpp_resource_type = match res.resource_type.as_str() {
            "Texture" => "TextureResource",
            "Mesh" => "MeshResource",
            _ => {
                // Unknown resource type - use as-is (might be custom)
                &res.resource_type
            }
        };
        
        // Generate: Resource<TextureResource> g_resource_MyTexture("path/to/file.dds");
        // Use lowercase name for the global variable (HEIDIC convention)
        let global_name = format!("g_resource_{}", res.name.to_lowercase());
        format!("Resource<{}> {}(\"{}\");\n", cpp_resource_type, global_name, res.path)
    }
    
    fn generate_resource_accessor(&self, res: &ResourceDef) -> String {
        // Generate accessor function so resource can be accessed in HEIDIC
        // Example: extern "C" Resource<MeshResource>* get_resource_mymesh() { return &g_resource_mymesh; }
        let global_name = format!("g_resource_{}", res.name.to_lowercase());
        let cpp_resource_type = match res.resource_type.as_str() {
            "Texture" => "TextureResource",
            "Mesh" => "MeshResource",
            _ => &res.resource_type
        };
        
        // Generate extern C function for HEIDIC access
        format!(
            "extern \"C\" Resource<{}>* get_resource_{}() {{ return &{}; }}\n",
            cpp_resource_type, res.name.to_lowercase(), global_name
        )
    }
    
    fn is_component_soa(&self, component_name: &str) -> bool {
        self.components.get(component_name)
            .map(|c| c.is_soa)
            .unwrap_or(false)
    }
    
    fn generate_function(&self, f: &FunctionDef, indent: usize) -> String {
        let mut output = String::new();
        
        // Rename HEIDIC main to avoid conflict with C++ main
        let func_name = if f.name == "main" {
            "heidic_main".to_string()
        } else {
            f.name.clone()
        };
        
        // If it's the main function with void return, change to int for C++
        let return_type = if f.name == "main" && matches!(f.return_type, Type::Void) {
            "int".to_string()
        } else {
            self.type_to_cpp(&f.return_type)
        };
        
        output.push_str(&format!("{} {}(", return_type, func_name));
        
        // Parameters
        for (i, param) in f.params.iter().enumerate() {
            if i > 0 {
                output.push_str(", ");
            }
            output.push_str(&format!("{} {}", 
                self.type_to_cpp(&param.ty), 
                param.name));
        }
        output.push_str(") {\n");
        
        // Inject ECS initialization if we have hot components and this is main
        if f.name == "main" && !self.hot_components.is_empty() {
            let mut injected_ecs = false;
            for (_i, stmt) in f.body.iter().enumerate() {
                output.push_str(&self.generate_statement(stmt, indent + 1));
                
                // After ball_count assignment, inject ECS initialization
                if !injected_ecs {
                    if let Statement::Let { name, .. } = stmt {
                        if name == "ball_count" {
                            // CRITICAL: Add debug IMMEDIATELY after ball_count to verify this code executes
                            // Use same indentation as surrounding statements (indent + 1 = 1 = 4 spaces for main)
                            let ecs_indent = self.indent(indent + 1);
                            output.push_str(&format!("{}\n", ecs_indent));
                            output.push_str(&format!("{}    // ========== ECS INITIALIZATION START ==========\n", ecs_indent));
                            output.push_str(&format!("{}    try {{\n", ecs_indent));
                            output.push_str(&format!("{}        std::cout << \"\\n=== [ECS] Starting entity creation... ===\\n\" << std::endl;\n", ecs_indent));
                            output.push_str(&format!("{}        std::cout.flush();\n", ecs_indent));
                            output.push_str(&format!("{}\n", ecs_indent));
                            output.push_str(&format!("{}        // Create entities with hot components in ECS\n", ecs_indent));
                            output.push_str(&format!("{}        g_entities.clear();\n", ecs_indent));
                            output.push_str(&format!("{}        const float init_pos[][3] = {{\n", ecs_indent));
                            output.push_str(&format!("{}            {{0.0f, 0.0f, 0.0f}},\n", ecs_indent));
                            output.push_str(&format!("{}            {{1.5f, 0.5f, -1.0f}},\n", ecs_indent));
                            output.push_str(&format!("{}            {{-1.0f, 1.0f, 0.5f}},\n", ecs_indent));
                            output.push_str(&format!("{}            {{0.5f, -1.2f, 1.0f}},\n", ecs_indent));
                            output.push_str(&format!("{}            {{-1.5f, -0.5f, -1.5f}},\n", ecs_indent));
                            output.push_str(&format!("{}        }};\n", ecs_indent));
                            output.push_str(&format!("{}        const float init_vel[][3] = {{\n", ecs_indent));
                            output.push_str(&format!("{}            {{1.0f, 0.5f, 0.3f}},\n", ecs_indent));
                            output.push_str(&format!("{}            {{-0.8f, 0.6f, -0.4f}},\n", ecs_indent));
                            output.push_str(&format!("{}            {{0.4f, -0.7f, 0.5f}},\n", ecs_indent));
                            output.push_str(&format!("{}            {{0.6f, 0.8f, -0.3f}},\n", ecs_indent));
                            output.push_str(&format!("{}            {{-0.5f, -0.4f, 0.7f}},\n", ecs_indent));
                            output.push_str(&format!("{}        }};\n", ecs_indent));
                            output.push_str(&format!("{}        for (int i = 0; i < ball_count; ++i) {{\n", ecs_indent));
                            output.push_str(&format!("{}            EntityId e = g_storage.create_entity();\n", ecs_indent));
                            output.push_str(&format!("{}            g_entities.push_back(e);\n", ecs_indent));
                            
                            // Generate component initialization based on hot components
                            for comp in &self.hot_components {
                                if comp.name == "Position" {
                                    output.push_str(&format!("{}            {} p{{init_pos[i][0], init_pos[i][1], init_pos[i][2]", ecs_indent, comp.name));
                                    // Add default values for additional fields
                                    for field in &comp.fields {
                                        if field.name != "x" && field.name != "y" && field.name != "z" {
                                            if field.name == "size" {
                                                output.push_str(", 0.2f");
                                            } else {
                                                output.push_str(", 0.0f");
                                            }
                                        }
                                    }
                                    output.push_str("};\n");
                                    output.push_str(&format!("{}            g_storage.add_component<{}>(e, p);\n", ecs_indent, comp.name));
                                } else if comp.name == "Velocity" {
                                    output.push_str(&format!("{}            {} v{{init_vel[i][0], init_vel[i][1], init_vel[i][2]}};\n", ecs_indent, comp.name));
                                    output.push_str(&format!("{}            g_storage.add_component<{}>(e, v);\n", ecs_indent, comp.name));
                                }
                            }
                            
                            output.push_str(&format!("{}        }}\n", ecs_indent));
                            output.push_str(&format!("{}        std::cout << \"=== [ECS] Created \" << ball_count << \" entities (g_entities.size()=\" << g_entities.size() << \") ===\\n\" << std::endl;\n", ecs_indent));
                            output.push_str(&format!("{}        std::cout.flush();\n", ecs_indent));
                            output.push_str(&format!("{}        std::cout << \"[ECS Init] g_entities.size()=\" << g_entities.size() << std::endl;\n", ecs_indent));
                            output.push_str(&format!("{}        if (!g_entities.empty()) {{\n", ecs_indent));
                            output.push_str(&format!("{}            auto* p = g_storage.get_component<Position>(g_entities[0]);\n", ecs_indent));
                            output.push_str(&format!("{}            auto* v = g_storage.get_component<Velocity>(g_entities[0]);\n", ecs_indent));
                            output.push_str(&format!("{}            if (p && v) {{\n", ecs_indent));
                            output.push_str(&format!("{}                std::cout << \"[ECS Init] Entity 0: pos=(\" << p->x << \",\" << p->y << \",\" << p->z << \") vel=(\" << v->x << \",\" << v->y << \",\" << v->z << \")\" << std::endl;\n", ecs_indent));
                            output.push_str(&format!("{}            }} else {{\n", ecs_indent));
                            output.push_str(&format!("{}                std::cout << \"[ECS Init] ERROR: Entity 0 missing components!\" << std::endl;\n", ecs_indent));
                            output.push_str(&format!("{}            }}\n", ecs_indent));
                            output.push_str(&format!("{}        }}\n", ecs_indent));
                            output.push_str(&format!("{}    }} catch (const std::exception& e) {{\n", ecs_indent));
                            output.push_str(&format!("{}        std::cout << \"[ECS ERROR] Exception: \" << e.what() << std::endl;\n", ecs_indent));
                            output.push_str(&format!("{}    }} catch (...) {{\n", ecs_indent));
                            output.push_str(&format!("{}        std::cout << \"[ECS ERROR] Unknown exception in ECS initialization!\" << std::endl;\n", ecs_indent));
                            output.push_str(&format!("{}    }}\n", ecs_indent));
                            injected_ecs = true;
                        }
                    }
                }
            }
        } else {
            // Normal generation without ECS injection
            for stmt in &f.body {
                output.push_str(&self.generate_statement(stmt, indent + 1));
            }
        }
        
        // If it's main with void return type, add return 0
        if f.name == "main" && matches!(f.return_type, Type::Void) {
            output.push_str(&format!("{}    return 0;\n", self.indent(indent + 1)));
        }
        
        output.push_str("}\n\n");
        output
    }
    
    fn generate_statement_with_entity(&self, stmt: &Statement, indent: usize, entity_name: &str, query_name: &str) -> String {
        // Generate statement but replace entity.Component.field with query.component_arrays[entity_index].field
        match stmt {
            Statement::Let { name, ty, value } => {
                // Handle let statements with entity access in value
                let type_str = if let Some(t) = ty {
                    format!("{} ", self.type_to_cpp(t))
                } else {
                    String::new()
                };
                let value_str = self.generate_expression_with_entity(value, entity_name, query_name);
                format!("{}    {} {}= {};\n", self.indent(indent), type_str, name, value_str)
            }
            Statement::Assign { target, value } => {
                // Handle entity.Component.field = value
                let target_str = self.generate_expression_with_entity(target, entity_name, query_name);
                let value_str = self.generate_expression_with_entity(value, entity_name, query_name);
                format!("{}    {} = {};\n", self.indent(indent), target_str, value_str)
            }
            _ => {
                // For other statements, use regular generation but with entity context
                self.generate_statement_with_entity_fallback(stmt, indent, entity_name, query_name)
            }
        }
    }
    
    fn generate_statement_with_entity_fallback(&self, stmt: &Statement, indent: usize, entity_name: &str, query_name: &str) -> String {
        // Fallback for statements that need entity context but aren't handled above
        match stmt {
            Statement::Expression(expr) => {
                format!("{}    {};\n",
                    self.indent(indent),
                    self.generate_expression_with_entity(expr, entity_name, query_name))
            }
            _ => {
                // For other statements, use regular generation
                self.generate_statement(stmt, indent)
            }
        }
    }
    
    fn generate_expression_with_entity(&self, expr: &Expression, entity_name: &str, query_name: &str) -> String {
        match expr {
            Expression::MemberAccess { object, member } => {
                // Check if this is entity.Component.field pattern
                if let Expression::MemberAccess { object: inner_obj, member: component_name } = object.as_ref() {
                    // This is entity.Component.field (nested member access)
                    if let Expression::Variable(var_name) = inner_obj.as_ref() {
                        if var_name == entity_name {
                            // This is entity.Component.field - generate query access
                            // Check if component is SOA
                            let is_soa = self.is_component_soa(component_name);
                            
                            // Convert to lowercase and pluralize (Position -> positions, Velocity -> velocities)
                            let component_lower = component_name.to_lowercase();
                            let component_plural = if component_lower.ends_with('y') {
                                // Velocity -> velocities (y -> ies)
                                format!("{}ies", &component_lower[..component_lower.len()-1])
                            } else if component_lower.ends_with('s') || component_lower.ends_with('x') || component_lower.ends_with('z') || component_lower.ends_with('h') {
                                format!("{}es", component_lower)
                            } else {
                                format!("{}s", component_lower)
                            };
                            
                            // Generate access pattern based on SOA vs AoS
                            if is_soa {
                                // SOA: query.velocities.x[entity_index] (field is array, index at end)
                                format!("{}.{}.{}[{}_index]", query_name, component_plural, member, entity_name)
                            } else {
                                // AoS: query.positions[entity_index].x (index first, then field)
                                format!("{}.{}[{}_index].{}", query_name, component_plural, entity_name, member)
                            }
                        } else {
                            // Not entity access, use regular generation
                            let obj_expr = self.generate_expression_with_entity(object, entity_name, query_name);
                            format!("{}.{}", obj_expr, member)
                        }
                    } else {
                        // Not entity.Component.field, use regular generation
                        let obj_expr = self.generate_expression_with_entity(object, entity_name, query_name);
                        format!("{}.{}", obj_expr, member)
                    }
                } else {
                    // Single level member access, check if object is entity.Component
                    let obj_expr = self.generate_expression_with_entity(object, entity_name, query_name);
                    if obj_expr == entity_name {
                        // This is entity.Component (without field) - shouldn't happen in valid code
                        format!("{}.{}", obj_expr, member)
                    } else {
                        format!("{}.{}", obj_expr, member)
                    }
                }
            }
            Expression::Variable(name) => {
                if name == entity_name {
                    // Entity variable itself - not used directly, but keep for now
                    name.clone()
                } else {
                    name.clone()
                }
            }
            Expression::BinaryOp { op, left, right } => {
                let op_str = match op {
                    BinaryOp::Add => "+",
                    BinaryOp::Sub => "-",
                    BinaryOp::Mul => "*",
                    BinaryOp::Div => "/",
                    BinaryOp::Mod => "%",
                    BinaryOp::Eq => "==",
                    BinaryOp::Ne => "!=",
                    BinaryOp::Lt => "<",
                    BinaryOp::Le => "<=",
                    BinaryOp::Gt => ">",
                    BinaryOp::Ge => ">=",
                    BinaryOp::And => "&&",
                    BinaryOp::Or => "||",
                };
                format!("({} {} {})", 
                    self.generate_expression_with_entity(left, entity_name, query_name),
                    op_str,
                    self.generate_expression_with_entity(right, entity_name, query_name))
            }
            Expression::Literal(lit) => {
                match lit {
                    Literal::Int(n) => n.to_string(),
                    Literal::Float(n) => n.to_string(),
                    Literal::Bool(b) => b.to_string(),
                    Literal::String(s) => format!("\"{}\"", s),
                }
            }
            _ => self.generate_expression(expr)
        }
    }
    
    fn generate_statement(&self, stmt: &Statement, indent: usize) -> String {
        match stmt {
            Statement::Let { name, ty, value } => {
                let type_str = if let Some(ty) = ty {
                    self.type_to_cpp(ty)
                } else {
                    "auto".to_string()
                };
                let mut output = format!("{}    {} {} = {};\n", 
                    self.indent(indent),
                    type_str,
                    name,
                    self.generate_expression(value));
                
                // Special case: Add immediate debug after ball_count to verify execution
                if name == "ball_count" && !self.hot_components.is_empty() {
                    output.push_str(&format!("{}    std::cout << \"[IMMEDIATE DEBUG] ball_count just set to \" << {} << std::endl;\n", 
                        self.indent(indent), name));
                    output.push_str(&format!("{}    std::cout.flush();\n", self.indent(indent)));
                }
                
                output
            }
            Statement::Assign { target, value } => {
                format!("{}    {} = {};\n",
                    self.indent(indent),
                    self.generate_expression(target),
                    self.generate_expression(value))
            }
            Statement::If { condition, then_block, else_block } => {
                let mut output = format!("{}    if ({}) {{\n", 
                    self.indent(indent),
                    self.generate_expression(condition));
                for stmt in then_block {
                    output.push_str(&self.generate_statement(stmt, indent + 1));
                }
                if let Some(else_block) = else_block {
                    output.push_str(&format!("{}    }} else {{\n", self.indent(indent)));
                    for stmt in else_block {
                        output.push_str(&self.generate_statement(stmt, indent + 1));
                    }
                }
                output.push_str(&format!("{}    }}\n", self.indent(indent)));
                output
            }
            Statement::While { condition, body } => {
                let mut output = format!("{}    while ({}) {{\n", 
                    self.indent(indent),
                    self.generate_expression(condition));
                // Add hot-reload check at the start of while loop if we have hot systems or hot shaders
                if !self.hot_systems.is_empty() {
                    // Add check at the start of each while loop iteration
                    output.push_str(&format!("{}        check_and_reload_hot_system();\n", self.indent(indent + 1)));
                }
                if !self.hot_shaders.is_empty() {
                    // Add shader hot-reload check at the start of each while loop iteration
                    output.push_str(&format!("{}        check_and_reload_hot_shaders();\n", self.indent(indent + 1)));
                }
                if !self.hot_components.is_empty() {
                    // Add component hot-reload check at the start of each while loop iteration
                    output.push_str(&format!("{}        check_and_migrate_hot_components();\n", self.indent(indent + 1)));
                }
                if self.has_resources {
                    // Add resource hot-reload check at the start of each while loop iteration
                    output.push_str(&format!("{}        check_and_reload_resources();\n", self.indent(indent + 1)));
                }
                for stmt in body {
                    output.push_str(&self.generate_statement(stmt, indent + 1));
                }
                output.push_str(&format!("{}    }}\n", self.indent(indent)));
                output
            }
            Statement::For { iterator, collection, body } => {
                // Generate query iteration: for entity in q { ... }
                let collection_expr = self.generate_expression(collection);
                
                // Generate iteration loop with index variable
                let mut output = format!("{}    // Query iteration: for {} in {}\n", 
                    self.indent(indent), iterator, collection_expr);
                output.push_str(&format!("{}    for (size_t {}_index = 0; {}_index < {}.size(); ++{}_index) {{\n",
                    self.indent(indent), iterator, iterator, collection_expr, iterator));
                
                // Generate body - entity access will be handled in expression generation
                // We need to track that we're in a query loop for entity access
                for stmt in body {
                    // Replace entity.Component.field with query.component_arrays[entity_index].field
                    output.push_str(&self.generate_statement_with_entity(stmt, indent + 1, iterator, &collection_expr));
                }
                output.push_str(&format!("{}    }}\n", self.indent(indent)));
                output
            }
            Statement::Loop { body } => {
                let mut output = format!("{}    while (true) {{\n", self.indent(indent));
                for stmt in body {
                    output.push_str(&self.generate_statement(stmt, indent + 1));
                }
                output.push_str(&format!("{}    }}\n", self.indent(indent)));
                output
            }
            Statement::Return(expr) => {
                if let Some(expr) = expr {
                    format!("{}    return {};\n",
                        self.indent(indent),
                        self.generate_expression(expr))
                } else {
                    format!("{}    return 0;\n", self.indent(indent))
                }
            }
            Statement::Expression(expr) => {
                let expr_str = self.generate_expression(expr);
                // If this is a call to heidic_render_balls and we have hot components, wrap it with ECS code
                if !self.hot_components.is_empty() && expr_str.contains("heidic_render_balls") {
                    let mut output = String::new();
                    // Extract ball_count from the call - for now, assume it's the second argument
                    output.push_str(&format!("{}            \n", self.indent(indent)));
                    output.push_str(&format!("{}            // Update physics using ECS (integrate positions with velocities)\n", self.indent(indent)));
                    output.push_str(&format!("{}            auto now = std::chrono::high_resolution_clock::now();\n", self.indent(indent)));
                    output.push_str(&format!("{}            auto dt_us = std::chrono::duration_cast<std::chrono::microseconds>(now - g_last_update_time);\n", self.indent(indent)));
                    output.push_str(&format!("{}            float dt = dt_us.count() / 1'000'000.0f;\n", self.indent(indent)));
                    output.push_str(&format!("{}            if (dt > 0.1f) dt = 0.016f; // clamp\n", self.indent(indent)));
                    output.push_str(&format!("{}            g_last_update_time = now;\n", self.indent(indent)));
                    output.push_str(&format!("{}            \n", self.indent(indent)));
                    output.push_str(&format!("{}            float speed_scale = 1.0f;\n", self.indent(indent)));
                    // Check if we have get_movement_speed hot function
                    let has_speed_func = self.hot_systems.iter().any(|s| {
                        s.functions.iter().any(|f| f.name == "get_movement_speed")
                    });
                    if has_speed_func {
                        output.push_str(&format!("{}            if (g_get_movement_speed) {{\n", self.indent(indent)));
                        output.push_str(&format!("{}                speed_scale = g_get_movement_speed();\n", self.indent(indent)));
                        output.push_str(&format!("{}            }}\n", self.indent(indent)));
                    }
                    output.push_str(&format!("{}            \n", self.indent(indent)));
                    output.push_str(&format!("{}            // Update positions using velocities from ECS\n", self.indent(indent)));
                    output.push_str(&format!("{}            for (EntityId e : g_entities) {{\n", self.indent(indent)));
                    // Generate component access based on hot components
                    let has_position = self.hot_components.iter().any(|c| c.name == "Position");
                    let has_velocity = self.hot_components.iter().any(|c| c.name == "Velocity");
                    if has_position && has_velocity {
                        output.push_str(&format!("{}                auto* p = g_storage.get_component<Position>(e);\n", self.indent(indent)));
                        output.push_str(&format!("{}                auto* v = g_storage.get_component<Velocity>(e);\n", self.indent(indent)));
                        output.push_str(&format!("{}                if (!p || !v) continue;\n", self.indent(indent)));
                        output.push_str(&format!("{}                \n", self.indent(indent)));
                        output.push_str(&format!("{}                // Integrate: pos += vel * dt * speed_scale\n", self.indent(indent)));
                        output.push_str(&format!("{}                p->x += v->x * dt * speed_scale;\n", self.indent(indent)));
                        output.push_str(&format!("{}                p->y += v->y * dt * speed_scale;\n", self.indent(indent)));
                        output.push_str(&format!("{}                p->z += v->z * dt * speed_scale;\n", self.indent(indent)));
                        output.push_str(&format!("{}                \n", self.indent(indent)));
                        output.push_str(&format!("{}                // Bounce off walls\n", self.indent(indent)));
                        output.push_str(&format!("{}                auto bounce_axis = [&](float& pos, float& vel) {{\n", self.indent(indent)));
                        output.push_str(&format!("{}                    if (pos > BOUNDS || pos < -BOUNDS) {{\n", self.indent(indent)));
                        output.push_str(&format!("{}                        vel = -vel;\n", self.indent(indent)));
                        output.push_str(&format!("{}                        pos = (pos > BOUNDS) ? BOUNDS : -BOUNDS;\n", self.indent(indent)));
                        output.push_str(&format!("{}                    }}\n", self.indent(indent)));
                        output.push_str(&format!("{}                }};\n", self.indent(indent)));
                        output.push_str(&format!("{}                bounce_axis(p->x, v->x);\n", self.indent(indent)));
                        output.push_str(&format!("{}                bounce_axis(p->y, v->y);\n", self.indent(indent)));
                        output.push_str(&format!("{}                bounce_axis(p->z, v->z);\n", self.indent(indent)));
                    }
                    output.push_str(&format!("{}            }}\n", self.indent(indent)));
                    output.push_str(&format!("{}            \n", self.indent(indent)));
                    output.push_str(&format!("{}            // Build arrays for renderer from ECS data\n", self.indent(indent)));
                    output.push_str(&format!("{}            std::vector<float> positions;\n", self.indent(indent)));
                    output.push_str(&format!("{}            positions.reserve(ball_count * 3);\n", self.indent(indent)));
                    output.push_str(&format!("{}            std::vector<float> sizes;\n", self.indent(indent)));
                    output.push_str(&format!("{}            sizes.reserve(ball_count);\n", self.indent(indent)));
                    output.push_str(&format!("{}            for (EntityId e : g_entities) {{\n", self.indent(indent)));
                    if has_position {
                        output.push_str(&format!("{}                auto* p = g_storage.get_component<Position>(e);\n", self.indent(indent)));
                        output.push_str(&format!("{}                if (!p) {{\n", self.indent(indent)));
                        output.push_str(&format!("{}                    positions.insert(positions.end(), {{0.0f, 0.0f, 0.0f}});\n", self.indent(indent)));
                        output.push_str(&format!("{}                    sizes.push_back(0.2f);\n", self.indent(indent)));
                        output.push_str(&format!("{}                    continue;\n", self.indent(indent)));
                        output.push_str(&format!("{}                }}\n", self.indent(indent)));
                        output.push_str(&format!("{}                positions.push_back(p->x);\n", self.indent(indent)));
                        output.push_str(&format!("{}                positions.push_back(p->y);\n", self.indent(indent)));
                        output.push_str(&format!("{}                positions.push_back(p->z);\n", self.indent(indent)));
                        // Check if Position has a size field
                        let pos_has_size = self.hot_components.iter()
                            .find(|c| c.name == "Position")
                            .map(|c| c.fields.iter().any(|f| f.name == "size"))
                            .unwrap_or(false);
                        if pos_has_size {
                            output.push_str(&format!("{}                sizes.push_back(p->size > 0.0f ? p->size : 0.2f);\n", self.indent(indent)));
                        } else {
                            output.push_str(&format!("{}                sizes.push_back(0.2f);\n", self.indent(indent)));
                        }
                    }
                    output.push_str(&format!("{}            }}\n", self.indent(indent)));
                    output.push_str(&format!("{}            \n", self.indent(indent)));
                    // Replace heidic_render_balls call with version that takes positions/sizes
                    let new_call = expr_str.replace("heidic_render_balls(window, ball_count)", 
                        "heidic_render_balls(window, ball_count, positions.data(), sizes.data())");
                    output.push_str(&format!("{}            {};\n", self.indent(indent), new_call));
                    output
                } else {
                    format!("{}    {};\n", self.indent(indent), expr_str)
                }
            }
            Statement::Block(stmts) => {
                let mut output = format!("{}    {{\n", self.indent(indent));
                for stmt in stmts {
                    output.push_str(&self.generate_statement(stmt, indent + 1));
                }
                output.push_str(&format!("{}    }}\n", self.indent(indent)));
                output
            }
        }
    }
    
    fn generate_expression(&self, expr: &Expression) -> String {
        match expr {
            Expression::Literal(lit) => {
                match lit {
                    Literal::Int(n) => n.to_string(),
                    Literal::Float(n) => n.to_string(),
                    Literal::Bool(b) => b.to_string(),
                    Literal::String(s) => format!("\"{}\"", s),
                }
            }
            Expression::Variable(name) => name.clone(),
            Expression::BinaryOp { op, left, right } => {
                let op_str = match op {
                    BinaryOp::Add => "+",
                    BinaryOp::Sub => "-",
                    BinaryOp::Mul => "*",
                    BinaryOp::Div => "/",
                    BinaryOp::Mod => "%",
                    BinaryOp::Eq => "==",
                    BinaryOp::Ne => "!=",
                    BinaryOp::Lt => "<",
                    BinaryOp::Le => "<=",
                    BinaryOp::Gt => ">",
                    BinaryOp::Ge => ">=",
                    BinaryOp::And => "&&",
                    BinaryOp::Or => "||",
                };
                format!("({} {} {})", 
                    self.generate_expression(left),
                    op_str,
                    self.generate_expression(right))
            }
            Expression::UnaryOp { op, expr } => {
                let op_str = match op {
                    UnaryOp::Neg => "-",
                    UnaryOp::Not => "!",
                };
                format!("{}({})", op_str, self.generate_expression(expr))
            }
            Expression::Call { name, args } => {
                // Check if this is a hot-reloadable function
                let is_hot = self.hot_systems.iter().any(|s| {
                    s.functions.iter().any(|f| f.name == *name)
                });
                
                if is_hot {
                    // Use function pointer for hot-reloadable functions
                    let mut output = format!("g_{}(", name);
                    for (i, arg) in args.iter().enumerate() {
                        if i > 0 {
                            output.push_str(", ");
                        }
                        output.push_str(&self.generate_expression(arg));
                    }
                    output.push_str(")");
                    return output;
                }
                
                // Handle built-in print function
                if name == "print" {
                    let mut output = String::from("std::cout");
                    for arg in args {
                        output.push_str(" << ");
                        output.push_str(&self.generate_expression(arg));
                    }
                    output.push_str(" << std::endl");
                    return output;
                }
                
                // Handle ImGui function calls (convert to ImGui:: namespace)
                if name.starts_with("ImGui_") || name.starts_with("ImGui::") {
                    let imgui_name = if name.starts_with("ImGui_") {
                        format!("ImGui::{}", &name[6..])
                    } else {
                        name.clone()
                    };
                    let mut output = format!("{}(", imgui_name);
                    for (i, arg) in args.iter().enumerate() {
                        if i > 0 {
                            output.push_str(", ");
                        }
                        output.push_str(&self.generate_expression(arg));
                    }
                    output.push_str(")");
                    return output;
                }
                
                // Regular function call
                let mut output = format!("{}(", name);
                for (i, arg) in args.iter().enumerate() {
                    if i > 0 {
                        output.push_str(", ");
                    }
                    let arg_expr = self.generate_expression(arg);
                    
                    // For heidic_init_renderer_dds_quad, string literals are fine (auto-convert to const char*)
                    // If it's a string variable, we'd need .c_str(), but string literals work directly
                    // Handle string literals for GLFW functions that need const char*
                    if (name == "glfwCreateWindow" && i == 2) || 
                       (name == "glfwSetWindowTitle" && i == 1) ||
                       (name == "heidic_init_renderer_dds_quad" && i == 1) {
                        // String literals are fine as-is for const char*
                        // String variables would need .c_str() but user is passing literal
                        output.push_str(&arg_expr);
                    } else {
                        output.push_str(&arg_expr);
                    }
                }
                output.push_str(")");
                output
            }
            Expression::MemberAccess { object, member } => {
                // Handle entity.Component.field access
                // If object is an entity variable (from for loop), generate query access
                let obj_expr = self.generate_expression(object);
                
                // Check if this is entity.Component.field pattern
                // For now, generate simple member access - TODO: improve for query entities
                format!("{}.{}", obj_expr, member)
            }
            Expression::Index { array, index } => {
                format!("{}[{}]", 
                    self.generate_expression(array),
                    self.generate_expression(index))
            }
            Expression::StructLiteral { name, fields } => {
                let mut output = format!("{} {{", name);
                for (i, (field_name, value)) in fields.iter().enumerate() {
                    if i > 0 {
                        output.push_str(", ");
                    }
                    output.push_str(&format!(".{} = {}", 
                        field_name,
                        self.generate_expression(value)));
                }
                output.push_str("}");
                output
            }
        }
    }
    
    fn type_to_cpp_for_extern(&self, ty: &Type) -> String {
        // For extern C functions, use C-compatible types
        match ty {
            Type::String => "const char*".to_string(),
            _ => self.type_to_cpp(ty)
        }
    }
    
    fn type_to_cpp(&self, ty: &Type) -> String {
        match ty {
            Type::I32 => "int32_t".to_string(),
            Type::I64 => "int64_t".to_string(),
            Type::F32 => "float".to_string(),
            Type::F64 => "double".to_string(),
            Type::Bool => "bool".to_string(),
            Type::String => "std::string".to_string(),
            Type::Array(element_type) => {
                format!("std::vector<{}>", self.type_to_cpp(element_type))
            }
            Type::Struct(name) => name.clone(),
            Type::Component(name) => name.clone(),
            Type::Query(component_types) => {
                // Generate query type name: Query_Position_Velocity
                let mut query_name = "Query_".to_string();
                for (i, ty) in component_types.iter().enumerate() {
                    if i > 0 {
                        query_name.push_str("_");
                    }
                    match ty {
                        Type::Component(name) => query_name.push_str(name),
                        Type::Struct(name) => query_name.push_str(name),
                        _ => query_name.push_str("Unknown"),
                    }
                }
                query_name
            }
            Type::Void => "void".to_string(),
            // Vulkan types
            Type::VkInstance => "VkInstance".to_string(),
            Type::VkDevice => "VkDevice".to_string(),
            Type::VkResult => "VkResult".to_string(),
            Type::VkPhysicalDevice => "VkPhysicalDevice".to_string(),
            Type::VkQueue => "VkQueue".to_string(),
            Type::VkCommandPool => "VkCommandPool".to_string(),
            Type::VkCommandBuffer => "VkCommandBuffer".to_string(),
            Type::VkSwapchainKHR => "VkSwapchainKHR".to_string(),
            Type::VkSurfaceKHR => "VkSurfaceKHR".to_string(),
            Type::VkRenderPass => "VkRenderPass".to_string(),
            Type::VkPipeline => "VkPipeline".to_string(),
            Type::VkFramebuffer => "VkFramebuffer".to_string(),
            Type::VkBuffer => "VkBuffer".to_string(),
            Type::VkImage => "VkImage".to_string(),
            Type::VkImageView => "VkImageView".to_string(),
            Type::VkSemaphore => "VkSemaphore".to_string(),
            Type::VkFence => "VkFence".to_string(),
            // GLFW types - GLFWwindow is already a pointer type in GLFW
            Type::GLFWwindow => "GLFWwindow*".to_string(), // GLFWwindow is GLFWwindow* in C
            Type::GLFWbool => "int32_t".to_string(), // GLFWbool is int32_t
            // Math types (mapped to GLM via stdlib/math.h)
            Type::Vec2 => "Vec2".to_string(),
            Type::Vec3 => "Vec3".to_string(),
            Type::Vec4 => "Vec4".to_string(),
            Type::Mat4 => "Mat4".to_string(),
        }
    }
    
    fn indent(&self, level: usize) -> String {
        "    ".repeat(level)
    }
}

