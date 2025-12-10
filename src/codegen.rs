use crate::ast::*;
use anyhow::Result;
use std::collections::HashMap;

pub struct CodeGenerator {
    components: HashMap<String, ComponentDef>,  // Store component metadata for SOA detection
    hot_systems: Vec<SystemDef>,  // Store hot-reloadable systems
    hot_shaders: Vec<ShaderDef>,  // Store hot-reloadable shaders
}

impl CodeGenerator {
    pub fn new() -> Self {
        Self {
            components: HashMap::new(),
            hot_systems: Vec::new(),
            hot_shaders: Vec::new(),
        }
    }
    
    pub fn generate(&mut self, program: &Program) -> Result<String> {
        let mut output = String::new();
        
        // Generate includes and standard library
        output.push_str("#include <iostream>\n");
        output.push_str("#include <vector>\n");
        output.push_str("#include <string>\n");
        output.push_str("#include <unordered_map>\n");
        output.push_str("#include <memory>\n");
        output.push_str("#include <cmath>\n");
        output.push_str("#include <cstdint>\n");
        output.push_str("\n");
        
        // Include EDEN standard library (Vulkan, GLFW, GLM math, ImGui)
        output.push_str("// EDEN ENGINE Standard Library\n");
        output.push_str("#include \"stdlib/vulkan.h\"\n");
        output.push_str("#include \"stdlib/glfw.h\"\n");
        output.push_str("#include \"stdlib/math.h\"\n");
        output.push_str("#include \"stdlib/imgui.h\"\n");
        output.push_str("\n");
        
        // First pass: collect component metadata (for SOA detection), hot systems, and hot shaders
        for item in &program.items {
            if let Item::Component(c) = item {
                self.components.insert(c.name.clone(), c.clone());
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
        
        // Generate extern function declarations (C linkage)
        let mut extern_libraries = std::collections::HashSet::new();
        for item in &program.items {
            if let Item::ExternFunction(ext) = item {
                output.push_str("extern \"C\" {\n");
                let return_type = self.type_to_cpp(&ext.return_type);
                output.push_str(&format!("    {} {}(", return_type, ext.name));
                for (i, param) in ext.params.iter().enumerate() {
                    if i > 0 {
                        output.push_str(", ");
                    }
                    output.push_str(&format!("{} {}", 
                        self.type_to_cpp(&param.ty), 
                        param.name));
                }
                output.push_str(");\n");
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
            output.push_str("\n");
            output.push_str("static time_t g_last_dll_time = 0;\n");
            output.push_str("\n");
            output.push_str("void check_and_reload_hot_system() {\n");
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
            for shader in &self.hot_shaders {
                // Get the shader file path (could be .glsl or .spv)
                let shader_path = &shader.path;
                // Determine the .spv path (replace .glsl with .spv, or append .spv if no extension)
                let spv_path = if shader_path.ends_with(".glsl") {
                    shader_path.replace(".glsl", ".spv")
                } else if shader_path.ends_with(".vert") {
                    shader_path.replace(".vert", ".spv")
                } else if shader_path.ends_with(".frag") {
                    shader_path.replace(".frag", ".spv")
                } else {
                    format!("{}.spv", shader_path)
                };
                
                output.push_str(&format!("    // Check {} shader file modification time\n", shader_path));
                output.push_str(&format!("    struct stat shader_stat;\n"));
                output.push_str(&format!("    if (stat(\"{}\", &shader_stat) == 0) {{\n", spv_path));
                output.push_str(&format!("        auto it = g_shader_mtimes.find(\"{}\");\n", spv_path));
                output.push_str(&format!("        time_t last_mtime = (it != g_shader_mtimes.end()) ? it->second : 0;\n"));
                output.push_str(&format!("        if (shader_stat.st_mtime > last_mtime) {{\n"));
                output.push_str(&format!("            g_shader_mtimes[\"{}\"] = shader_stat.st_mtime;\n", spv_path));
                output.push_str(&format!("            std::cout << \"[Shader Hot-Reload] Detected change in {}, reloading...\" << std::endl;\n", spv_path));
                output.push_str(&format!("            heidic_reload_shader(\"{}\");\n", spv_path));
                output.push_str(&format!("            std::cout << \"[Shader Hot-Reload] {} reloaded successfully!\" << std::endl;\n", spv_path));
                output.push_str(&format!("        }}\n"));
                output.push_str(&format!("    }}\n"));
            }
            output.push_str("}\n");
            output.push_str("\n");
            // Initialize shader modification times at startup
            output.push_str("static void init_shader_mtimes() {\n");
            for shader in &self.hot_shaders {
                let shader_path = &shader.path;
                let spv_path = if shader_path.ends_with(".glsl") {
                    shader_path.replace(".glsl", ".spv")
                } else if shader_path.ends_with(".vert") {
                    shader_path.replace(".vert", ".spv")
                } else if shader_path.ends_with(".frag") {
                    shader_path.replace(".frag", ".spv")
                } else {
                    format!("{}.spv", shader_path)
                };
                output.push_str(&format!("    struct stat shader_stat;\n"));
                output.push_str(&format!("    if (stat(\"{}\", &shader_stat) == 0) {{\n", spv_path));
                output.push_str(&format!("        g_shader_mtimes[\"{}\"] = shader_stat.st_mtime;\n", spv_path));
                output.push_str(&format!("    }}\n"));
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
        
        for stmt in &f.body {
            output.push_str(&self.generate_statement(stmt, indent + 1));
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
                format!("{}    {} {} = {};\n", 
                    self.indent(indent),
                    type_str,
                    name,
                    self.generate_expression(value))
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
                format!("{}    {};\n",
                    self.indent(indent),
                    self.generate_expression(expr))
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
                    // Handle string literals for GLFW functions that need const char*
                    if (name == "glfwCreateWindow" && i == 2) || 
                       (name == "glfwSetWindowTitle" && i == 1) {
                        // String literals are fine as-is for const char*
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

