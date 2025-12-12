use crate::ast::*;
use crate::error::{SourceLocation, ErrorReporter};
use anyhow::{Result, bail};
use std::collections::HashMap;

// Calculate Levenshtein distance between two strings
fn levenshtein_distance(s1: &str, s2: &str) -> usize {
    let s1_chars: Vec<char> = s1.chars().collect();
    let s2_chars: Vec<char> = s2.chars().collect();
    let n = s1_chars.len();
    let m = s2_chars.len();
    
    if n == 0 { return m; }
    if m == 0 { return n; }
    
    let mut dp = vec![vec![0; m + 1]; n + 1];
    
    for i in 0..=n {
        dp[i][0] = i;
    }
    for j in 0..=m {
        dp[0][j] = j;
    }
    
    for i in 1..=n {
        for j in 1..=m {
            let cost = if s1_chars[i - 1] == s2_chars[j - 1] { 0 } else { 1 };
            dp[i][j] = (dp[i - 1][j] + 1)
                .min(dp[i][j - 1] + 1)
                .min(dp[i - 1][j - 1] + cost);
        }
    }
    
    dp[n][m]
}

// Find the closest match in a list of candidates
fn find_closest_match(target: &str, candidates: &[String], max_distance: usize) -> Option<String> {
    let mut best_match: Option<(String, usize)> = None;
    
    for candidate in candidates {
        let distance = levenshtein_distance(target, candidate);
        if distance <= max_distance {
            if best_match.is_none() || distance < best_match.as_ref().unwrap().1 {
                best_match = Some((candidate.clone(), distance));
            }
        }
    }
    
    best_match.map(|(name, _)| name)
}

pub struct TypeChecker {
    symbols: HashMap<String, Type>,
    functions: HashMap<String, FunctionDef>,
    structs: HashMap<String, StructDef>,
    components: HashMap<String, ComponentDef>,
    errors: Vec<(SourceLocation, String, Option<String>)>,  // (location, message, suggestion)
    error_reporter: Option<ErrorReporter>,
    frame_scoped_vars: std::collections::HashSet<String>,  // Track variables allocated via frame.alloc_array
}

impl TypeChecker {
    pub fn new() -> Self {
        Self {
            symbols: HashMap::new(),
            functions: HashMap::new(),
            structs: HashMap::new(),
            components: HashMap::new(),
            errors: Vec::new(),
            error_reporter: None,
            frame_scoped_vars: std::collections::HashSet::new(),
        }
    }
    
    pub fn set_error_reporter(&mut self, reporter: ErrorReporter) {
        self.error_reporter = Some(reporter);
    }
    
    fn report_error(&mut self, location: SourceLocation, message: String, suggestion: Option<String>) {
        self.errors.push((location, message.clone(), suggestion.clone()));
        if let Some(ref reporter) = self.error_reporter {
            reporter.report_error(location, &message, suggestion.as_deref());
        }
    }
    
    fn report_error_with_secondary(
        &mut self, 
        location: SourceLocation, 
        message: String, 
        suggestion: Option<String>,
        secondary_location: Option<SourceLocation>,
        secondary_label: Option<&str>,
    ) {
        self.errors.push((location, message.clone(), suggestion.clone()));
        if let Some(ref reporter) = self.error_reporter {
            reporter.report_error_with_secondary(
                location, 
                &message, 
                suggestion.as_deref(),
                secondary_location,
                secondary_label,
            );
        }
    }
    
    pub fn check(&mut self, program: &Program) -> Result<()> {
        // Clear any previous errors
        self.errors.clear();
        
        // First pass: collect all definitions
        for item in &program.items {
            match item {
                Item::Struct(s) => {
                    self.structs.insert(s.name.clone(), s.clone());
                }
                Item::Component(c) => {
                    // Validate SOA components: all fields must be arrays
                    if c.is_soa {
                        for field in &c.fields {
                            if !matches!(field.ty, Type::Array(_)) {
                                let location = SourceLocation::unknown(); // TODO: get from AST
                                self.report_error(
                                    location,
                                    format!("SOA component '{}' field '{}' must be an array type (use [Type] instead of Type)", 
                                            c.name, field.name),
                                    Some(format!("Change '{}: {}' to '{}: [{}]'", 
                                                 field.name, 
                                                 self.type_to_string(&field.ty),
                                                 field.name,
                                                 self.type_to_string(&field.ty))),
                                );
                            }
                        }
                    }
                    self.components.insert(c.name.clone(), c.clone());
                }
                Item::Function(f) => {
                    self.functions.insert(f.name.clone(), f.clone());
                }
                Item::ExternFunction(ext) => {
                    // Create a function def from extern for type checking
                    let func_def = FunctionDef {
                        name: ext.name.clone(),
                        params: ext.params.clone(),
                        return_type: ext.return_type.clone(),
                        body: Vec::new(), // Extern functions have no body
                        cuda_kernel: None,
                    };
                    self.functions.insert(ext.name.clone(), func_def);
                }
                Item::System(s) => {
                    for func in &s.functions {
                        self.functions.insert(func.name.clone(), func.clone());
                    }
                }
                Item::Shader(shader) => {
                    // Validate that shader stage matches file extension
                    self.validate_shader_stage(shader)?;
                }
                Item::Resource(res) => {
                    // Resources don't need type checking - they're just declarations
                    // The resource type (Texture, Mesh) is validated at codegen time
                    // But we need to register the accessor function for type checking
                    let accessor_name = format!("get_resource_{}", res.name.to_lowercase());
                    let func_def = FunctionDef {
                        name: accessor_name.clone(),
                        params: Vec::new(), // No parameters
                        return_type: Type::I32, // Return pointer as i32 (opaque handle)
                        body: Vec::new(), // Generated function, no body
                        cuda_kernel: None,
                    };
                    self.functions.insert(accessor_name, func_def);
                    
                    // Register play/stop helper functions for audio resources
                    if res.resource_type == "Sound" || res.resource_type == "Music" {
                        let play_func_name = format!("play_resource_{}", res.name.to_lowercase());
                        let play_func = FunctionDef {
                            name: play_func_name.clone(),
                            params: Vec::new(),
                            return_type: Type::I32, // Returns 1 on success, 0 on failure
                            body: Vec::new(),
                            cuda_kernel: None,
                        };
                        self.functions.insert(play_func_name, play_func);
                        
                        let stop_func_name = format!("stop_resource_{}", res.name.to_lowercase());
                        let stop_func = FunctionDef {
                            name: stop_func_name.clone(),
                            params: Vec::new(),
                            return_type: Type::Void,
                            body: Vec::new(),
                            cuda_kernel: None,
                        };
                        self.functions.insert(stop_func_name, stop_func);
                    }
                }
                Item::Pipeline(_) => {
                    // Pipelines don't need type checking - they're just declarations
                    // Validation happens at codegen time (shader paths, binding types, etc.)
                }
            }
        }
        
        // Second pass: type check
        for item in &program.items {
            match item {
                Item::Function(f) => {
                    self.check_function(f)?;
                }
                Item::System(s) => {
                    for func in &s.functions {
                        self.check_function(func)?;
                    }
                }
                Item::Resource(_) => {
                    // Resources don't need type checking in second pass
                }
                Item::Pipeline(_) => {
                    // Pipelines don't need type checking in second pass
                }
                _ => {}
            }
        }
        
        // Report all errors if any
        if !self.errors.is_empty() {
            eprintln!("\n❌ Compilation failed with {} error(s):\n", self.errors.len());
            // Errors have already been printed by ErrorReporter, but we can add a summary
            bail!("Compilation failed with {} error(s). See errors above.", self.errors.len());
        }
        
        Ok(())
    }
    
    fn type_to_string(&self, ty: &Type) -> String {
        match ty {
            Type::I32 => "i32".to_string(),
            Type::I64 => "i64".to_string(),
            Type::F32 => "f32".to_string(),
            Type::F64 => "f64".to_string(),
            Type::Bool => "bool".to_string(),
            Type::String => "string".to_string(),
            Type::Array(elem) => format!("[{}]", self.type_to_string(elem)),
            Type::Optional(inner) => format!("?{}", self.type_to_string(inner)),
            Type::Struct(name) => name.clone(),
            Type::Component(name) => name.clone(),
            Type::Query(components) => {
                let comp_names: Vec<String> = components.iter()
                    .map(|c| self.type_to_string(c))
                    .collect();
                format!("query<{}>", comp_names.join(", "))
            },
            Type::Void => "void".to_string(),
            Type::Error => "<error>".to_string(),
            _ => format!("{:?}", ty),
        }
    }
    
    fn check_function(&mut self, func: &FunctionDef) -> Result<()> {
        self.symbols.clear();
        self.frame_scoped_vars.clear();  // Reset frame-scoped tracking for each function
        
        // Add parameters to symbol table
        for param in &func.params {
            self.symbols.insert(param.name.clone(), param.ty.clone());
        }
        
        // Store function return type for return statement validation
        let function_return_type = func.return_type.clone();
        
        // Check function body (continue even if errors occur)
        for stmt in &func.body {
            // Pass function return type to check_statement for return validation
            if let Err(_) = self.check_statement_with_return_type(stmt, &function_return_type) {
                // Continue checking other statements (error recovery)
            }
        }
        
        Ok(())
    }
    
    fn check_statement_with_return_type(&mut self, stmt: &Statement, expected_return_type: &Type) -> Result<()> {
        match stmt {
            Statement::Return(expr, location) => {
                if let Some(expr) = expr {
                    let return_type = match self.check_expression(expr) {
                        Ok(ty) => ty,
                        Err(_) => {
                            // Expression had error, continue checking
                            return Ok(());
                        }
                    };
                    
                    // If return type is Error, skip validation (already reported)
                    if !matches!(return_type, Type::Error) {
                        // Validate return type matches function return type
                        if !self.types_compatible(expected_return_type, &return_type) {
                            self.report_error(
                                *location,
                                format!("Return type mismatch: function returns '{}', but got '{}'", 
                                       self.type_to_string(expected_return_type),
                                       self.type_to_string(&return_type)),
                                Some(format!("Return a {} value: return <value>;", 
                                            self.type_to_string(expected_return_type))),
                            );
                        }
                    }
                    
                    // Check if returning a frame-scoped variable
                    if let Expression::Variable(var_name, _) = expr {
                        if self.frame_scoped_vars.contains(var_name) {
                            self.report_error(
                                *location,
                                format!("Cannot return frame-scoped allocation '{}': frame-scoped memory is only valid within the current frame", var_name),
                                Some(format!("Frame-scoped allocations (from frame.alloc_array) cannot be returned from functions. Consider using heap allocation or passing the FrameArena as a parameter.")),
                            );
                        }
                    } else if self.is_frame_alloc_expression(expr) {
                        // Returning the result of frame.alloc_array directly
                        self.report_error(
                            *location,
                            "Cannot return frame-scoped allocation: frame-scoped memory is only valid within the current frame".to_string(),
                            Some("Frame-scoped allocations (from frame.alloc_array) cannot be returned from functions. Consider using heap allocation or passing the FrameArena as a parameter.".to_string()),
                        );
                    }
                } else {
                    // Return without value - check if function expects void
                    if !matches!(expected_return_type, Type::Void) {
                        self.report_error(
                            *location,
                            format!("Function must return '{}', but return statement has no value", 
                                   self.type_to_string(expected_return_type)),
                            Some(format!("Return a {} value: return <value>;", 
                                        self.type_to_string(expected_return_type))),
                        );
                    }
                }
            }
            _ => {
                // For non-return statements, use regular check_statement
                self.check_statement(stmt)?;
            }
        }
        Ok(())
    }
    
    fn check_statement(&mut self, stmt: &Statement) -> Result<()> {
        match stmt {
            Statement::Let { name, ty, value, location } => {
                let value_type = self.check_expression(value)?;
                
                // Check if this is a frame-scoped allocation
                if self.is_frame_alloc_expression(value) {
                    self.frame_scoped_vars.insert(name.clone());
                }
                
                // If value type is Error, still add to symbol table as Error to allow recovery
                if let Some(declared_type) = ty {
                    if !self.types_compatible(declared_type, &value_type) && !matches!(value_type, Type::Error) {
                        let suggestion = format!("Use a {} variable or convert: {} = {}", 
                                                  self.type_to_string(declared_type),
                                                  name,
                                                  self.suggest_value_for_type(declared_type));
                        self.report_error(
                            *location,
                            format!("Type mismatch: cannot assign '{}' to '{}'", 
                                   self.type_to_string(&value_type),
                                   self.type_to_string(declared_type)),
                            Some(suggestion),
                        );
                    }
                    // Add declared type to symbol table (or Error if value was Error)
                    if matches!(value_type, Type::Error) {
                        self.symbols.insert(name.clone(), Type::Error);
                    } else {
                        self.symbols.insert(name.clone(), declared_type.clone());
                    }
                } else {
                    // Infer type from value (may be Error)
                    self.symbols.insert(name.clone(), value_type);
                }
            }
            Statement::Assign { target, value, location } => {
                let target_type = match self.check_expression(target) {
                    Ok(ty) => ty,
                    Err(_) => Type::Error,  // Continue checking value
                };
                let value_type = match self.check_expression(value) {
                    Ok(ty) => ty,
                    Err(_) => Type::Error,  // Continue checking
                };
                
                // If either is Error, skip type checking (already reported)
                if !matches!(target_type, Type::Error) && !matches!(value_type, Type::Error) {
                    if !self.types_compatible(&target_type, &value_type) {
                        let suggestion = format!("Ensure types match: {} should be {}", 
                                                self.type_to_string(&value_type),
                                                self.type_to_string(&target_type));
                        self.report_error(
                            *location,
                            format!("Type mismatch in assignment: cannot assign '{}' to '{}'", 
                                   self.type_to_string(&value_type),
                                   self.type_to_string(&target_type)),
                            Some(suggestion),
                        );
                    }
                }
            }
            Statement::If { condition, then_block, else_block, location } => {
                let cond_type = match self.check_expression(condition) {
                    Ok(ty) => ty,
                    Err(_) => Type::Error,  // Continue checking blocks
                };
                
                // If condition is Error, still check blocks (error recovery)
                if !matches!(cond_type, Type::Error) {
                    // Allow optional types in if conditions (truthiness check)
                    // if optional { ... } checks if optional has a value
                    let is_bool_or_optional = matches!(cond_type, Type::Bool) || matches!(cond_type, Type::Optional(_));
                    
                    if !is_bool_or_optional {
                        self.report_error(
                            *location,
                            format!("If condition must be bool or optional type, got '{}'", self.type_to_string(&cond_type)),
                            Some("Use a boolean expression: if (condition == true) or if (x > 0), or check optional: if optional { ... }".to_string()),
                        );
                    }
                }
                // Continue checking blocks even if condition had error
                for stmt in then_block {
                    if let Err(_) = self.check_statement(stmt) {
                        // Continue checking other statements
                    }
                }
                if let Some(else_block) = else_block {
                    for stmt in else_block {
                        if let Err(_) = self.check_statement(stmt) {
                            // Continue checking other statements
                        }
                    }
                }
            }
            Statement::While { condition, body, location } => {
                let cond_type = match self.check_expression(condition) {
                    Ok(ty) => ty,
                    Err(_) => Type::Error,  // Continue checking body
                };
                
                // If condition is Error, still check body (error recovery)
                if !matches!(cond_type, Type::Error) {
                    if !matches!(cond_type, Type::Bool) {
                        self.report_error(
                            *location,
                            format!("While condition must be bool, got '{}'", self.type_to_string(&cond_type)),
                            Some("Use a boolean expression: while (condition == true) or while (x > 0)".to_string()),
                        );
                    }
                }
                // Continue checking body even if condition had error
                for stmt in body {
                    if let Err(_) = self.check_statement(stmt) {
                        // Continue checking other statements
                    }
                }
            }
            Statement::For { iterator, collection, body, location } => {
                // Check that collection is a query type
                let collection_type = match self.check_expression(collection) {
                    Ok(ty) => ty,
                    Err(_) => Type::Error,  // Continue checking body
                };
                
                // If collection is Error, still check body (error recovery)
                if let Type::Query(component_types) = collection_type {
                    // Add iterator to symbol table as an "entity" type
                    // For now, we'll use a special marker - in codegen we'll handle entity access
                    // Store the query components for codegen
                    self.symbols.insert(iterator.clone(), Type::Query(component_types.clone()));
                    
                    // Check body with iterator in scope
                    for stmt in body {
                        if let Err(_) = self.check_statement(stmt) {
                            // Continue checking other statements
                        }
                    }
                    
                    // Remove iterator from scope after loop
                    self.symbols.remove(iterator);
                } else if !matches!(collection_type, Type::Error) {
                    // Only report error if collection type is not Error (Error already reported)
                    self.report_error(
                        *location,
                        format!("For loop collection must be a query type, got '{}'", self.type_to_string(&collection_type)),
                        Some("Use a query: for entity in query<Position, Velocity>".to_string()),
                    );
                }
            }
            Statement::Loop { body, .. } => {
                for stmt in body {
                    self.check_statement(stmt)?;
                }
            }
            Statement::Return(expr, location) => {
                // Return statement validation is now handled in check_statement_with_return_type
                // This is a fallback for statements checked outside of function context
                if let Some(expr) = expr {
                    self.check_expression(expr)?;
                    
                    // Check if returning a frame-scoped variable
                    if let Expression::Variable(var_name, _) = expr {
                        if self.frame_scoped_vars.contains(var_name) {
                            self.report_error(
                                *location,
                                format!("Cannot return frame-scoped allocation '{}': frame-scoped memory is only valid within the current frame", var_name),
                                Some(format!("Frame-scoped allocations (from frame.alloc_array) cannot be returned from functions. Consider using heap allocation or passing the FrameArena as a parameter.")),
                            );
                        }
                    } else if self.is_frame_alloc_expression(expr) {
                        self.report_error(
                            *location,
                            "Cannot return frame-scoped allocation: frame-scoped memory is only valid within the current frame".to_string(),
                            Some("Frame-scoped allocations (from frame.alloc_array) cannot be returned from functions. Consider using heap allocation or passing the FrameArena as a parameter.".to_string()),
                        );
                    }
                }
            }
            Statement::Expression(expr, ..) => {
                self.check_expression(expr)?;
            }
            Statement::Block(stmts, ..) => {
                for stmt in stmts {
                    if let Err(_) = self.check_statement(stmt) {
                        // Continue checking other statements (error recovery)
                    }
                }
            }
            Statement::Break(_) => {
                // Break statements don't need type checking
            }
            Statement::Continue(_) => {
                // Continue statements don't need type checking
            }
            Statement::Defer(expr, _) => {
                // Defer statements execute at scope exit - just check the expression
                if let Err(_) = self.check_expression(expr) {
                    // Continue (error recovery)
                }
            }
        }
        Ok(())
    }
    
    fn validate_shader_stage(&mut self, shader: &ShaderDef) -> Result<()> {
        use crate::ast::ShaderStage;
        
        // Determine expected extension based on stage
        let expected_ext = match shader.stage {
            ShaderStage::Vertex => ".vert",
            ShaderStage::Fragment => ".frag",
            ShaderStage::Compute => ".comp",
            ShaderStage::Geometry => ".geom",
            ShaderStage::TessellationControl => ".tesc",
            ShaderStage::TessellationEvaluation => ".tese",
        };
        
        // Check if path ends with expected extension
        let path_lower = shader.path.to_lowercase();
        let has_correct_ext = path_lower.ends_with(expected_ext);
        
        // Also check for .spv (compiled shader) - that's okay too
        let is_spv = path_lower.ends_with(".spv");
        
        // Allow .glsl extension (generic) - no validation in that case
        let is_generic = path_lower.ends_with(".glsl");
        
        if !has_correct_ext && !is_spv && !is_generic {
            let location = SourceLocation::unknown(); // TODO: get from AST
            let stage_name = match shader.stage {
                ShaderStage::Vertex => "vertex",
                ShaderStage::Fragment => "fragment",
                ShaderStage::Compute => "compute",
                ShaderStage::Geometry => "geometry",
                ShaderStage::TessellationControl => "tessellation_control",
                ShaderStage::TessellationEvaluation => "tessellation_evaluation",
            };
            
            self.report_error(
                location,
                format!(
                    "Shader stage '{}' does not match file extension. Expected '{}' extension for {} shader, but got '{}'",
                    stage_name,
                    expected_ext,
                    stage_name,
                    shader.path
                ),
                Some(format!(
                    "Change the file path to end with '{}' or use a .glsl extension for generic shaders",
                    expected_ext
                )),
            );
        }
        
        Ok(())
    }
    
    fn suggest_value_for_type(&self, ty: &Type) -> String {
        match ty {
            Type::I32 => "0".to_string(),
            Type::I64 => "0".to_string(),
            Type::F32 => "0.0".to_string(),
            Type::F64 => "0.0".to_string(),
            Type::Bool => "true".to_string(),
            Type::String => "\"\"".to_string(),
            _ => format!("/* {} value */", self.type_to_string(ty)),
        }
    }
    
    fn check_expression(&mut self, expr: &Expression) -> Result<Type> {
        match expr {
            Expression::Literal(lit, _) => {
                Ok(match lit {
                    Literal::Int(_) => Type::I32,
                    Literal::Float(_) => Type::F32,
                    Literal::Bool(_) => Type::Bool,
                    Literal::String(_) => Type::String,
                })
            }
            Expression::StringInterpolation { parts, location } => {
                // Validate all variables in interpolation exist and are valid types
                for part in parts {
                    if let crate::ast::StringInterpolationPart::Variable(var_name) = part {
                        // Check if variable exists
                        if let Some(var_type) = self.symbols.get(var_name) {
                            // Validate that the type can be converted to string
                            // Allow numeric types, bool, and string
                            match var_type {
                                Type::I32 | Type::I64 | Type::F32 | Type::F64 | Type::Bool | Type::String => {
                                    // These types can be converted to string
                                }
                                _ => {
                                    self.report_error(
                                        *location,
                                        format!("Variable '{}' has type '{}', which cannot be converted to string in interpolation", 
                                               var_name, self.type_to_string(var_type)),
                                        Some(format!("Use a numeric type (i32, i64, f32, f64), bool, or string for string interpolation")),
                                    );
                                    // Mark as error, will return Error type at end
                                    // (handled by has_error flag in the updated version)
                                }
                            }
                        } else {
                            // Find similar variable names
                            let candidates: Vec<String> = self.symbols.keys().cloned().collect();
                            let suggestion = if let Some(closest) = find_closest_match(var_name, &candidates, 3) {
                                format!("Did you mean '{}'? Use: {{}}", closest)
                            } else {
                                format!("Did you mean to declare it first? Use: let {}: Type = value;", var_name)
                            };
                            
                            self.report_error(
                                *location,
                                format!("Undefined variable '{}' in string interpolation", var_name),
                                Some(suggestion),
                            );
                            // Continue checking other parts, but mark as error
                            // We'll return Error type at the end if any errors occurred
                        }
                    }
                }
                Ok(Type::String)
            }
            Expression::Match { expr, arms, location: _ } => {
                // Type check the expression being matched
                let expr_type = self.check_expression(expr)?;
                
                // Validate all arms
                let mut _has_wildcard = false;
                
                for arm in arms {
                    // Type check the body
                    // Create a new scope for pattern variables
                    let old_symbols = self.symbols.clone();
                    
                    // If pattern binds a variable, add it to scope
                    if let crate::ast::Pattern::Variable(var_name, _) = &arm.pattern {
                        self.symbols.insert(var_name.clone(), expr_type.clone());
                    }
                    
                    // Check body statements
                    for stmt in &arm.body {
                        self.check_statement(stmt)?;
                    }
                    
                    // Restore symbols
                    self.symbols = old_symbols;
                    
                    // Check for wildcard
                    if matches!(arm.pattern, crate::ast::Pattern::Wildcard(_)) {
                        _has_wildcard = true;
                    }
                }
                
                // Warn if no wildcard and not exhaustive (for enums)
                // For now, just validate patterns are compatible
                
                // Return type is the common type of all arm bodies, or void if no return
                // For now, return void (match as statement)
                // TODO: Support match as expression with return types
                Ok(Type::Void)
            }
            Expression::Variable(name, location) => {
                match self.symbols.get(name) {
                    Some(ty) => Ok(ty.clone()),
                    None => {
                        // Find similar variable names
                        let candidates: Vec<String> = self.symbols.keys().cloned().collect();
                        let suggestion = if let Some(closest) = find_closest_match(name, &candidates, 3) {
                            format!("Did you mean '{}'? Use: {}", closest, closest)
                        } else {
                            format!("Did you mean to declare it first? Use: let {}: Type = value;", name)
                        };
                        
                        self.report_error(
                            *location,
                            format!("Undefined variable: '{}'", name),
                            Some(suggestion),
                        );
                        // Return Error type instead of bailing - allows error recovery
                        Ok(Type::Error)
                    }
                }
            }
            Expression::BinaryOp { op, left, right, location } => {
                let left_type = self.check_expression(left)?;
                let right_type = self.check_expression(right)?;
                
                // If either operand is Error, propagate Error
                if matches!(left_type, Type::Error) || matches!(right_type, Type::Error) {
                    return Ok(Type::Error);
                }
                
                match op {
                    BinaryOp::Add | BinaryOp::Sub | BinaryOp::Mul | BinaryOp::Div | BinaryOp::Mod => {
                        if matches!(left_type, Type::I32 | Type::I64 | Type::F32 | Type::F64) &&
                           matches!(right_type, Type::I32 | Type::I64 | Type::F32 | Type::F64) {
                            Ok(left_type) // Simplified: return left type
                        } else {
                            self.report_error(
                                *location,
                                format!("Arithmetic operations require numeric types, got '{}' and '{}'", 
                                       self.type_to_string(&left_type),
                                       self.type_to_string(&right_type)),
                                Some("Use numeric types (i32, i64, f32, f64) for arithmetic operations".to_string()),
                            );
                            // Return Error type instead of bailing - allows error recovery
                            Ok(Type::Error)
                        }
                    }
                    BinaryOp::Eq | BinaryOp::Ne | BinaryOp::Lt | BinaryOp::Le | BinaryOp::Gt | BinaryOp::Ge => {
                        Ok(Type::Bool)
                    }
                    BinaryOp::And | BinaryOp::Or => {
                        if matches!(left_type, Type::Bool) && matches!(right_type, Type::Bool) {
                            Ok(Type::Bool)
                        } else {
                            self.report_error(
                                *location,
                                format!("Logical operations require bool types, got '{}' and '{}'", 
                                       self.type_to_string(&left_type),
                                       self.type_to_string(&right_type)),
                                Some("Use bool types for logical operations (&&, ||)".to_string()),
                            );
                            // Return Error type instead of bailing - allows error recovery
                            Ok(Type::Error)
                        }
                    }
                }
            }
            Expression::UnaryOp { op, expr, location } => {
                let expr_type = self.check_expression(expr)?;
                match op {
                    UnaryOp::Neg => {
                        if matches!(expr_type, Type::I32 | Type::I64 | Type::F32 | Type::F64) {
                            Ok(expr_type)
                        } else {
                            self.report_error(
                                *location,
                                format!("Negation requires numeric type, got '{}'", self.type_to_string(&expr_type)),
                                Some("Use a numeric type (i32, i64, f32, f64) for negation".to_string()),
                            );
                            bail!("Negation requires numeric type");
                        }
                    }
                    UnaryOp::Not => {
                        if matches!(expr_type, Type::Bool) {
                            Ok(Type::Bool)
                        } else {
                            self.report_error(
                                *location,
                                format!("Not requires bool type, got '{}'", self.type_to_string(&expr_type)),
                                Some("Use a bool type for logical not (!)".to_string()),
                            );
                            bail!("Not requires bool type");
                        }
                    }
                }
            }
            Expression::Call { name, args, location } => {
                // Handle built-in print function
                if name == "print" {
                    // Print can take any number of arguments of any type
                    for arg in args {
                        self.check_expression(arg)?;
                    }
                    return Ok(Type::Void);
                }
                
                // Handle GLFW built-in functions
                let glfw_result = match name.as_str() {
                    "glfwInit" => {
                        if args.len() != 0 {
                            bail!("glfwInit() takes no arguments");
                        }
                        Ok(Type::I32)
                    }
                    "glfwCreateWindow" => {
                        if args.len() != 5 {
                            bail!("glfwCreateWindow() takes 5 arguments: width, height, title, monitor, share");
                        }
                        self.check_expression(&args[0])?; // width
                        self.check_expression(&args[1])?; // height
                        self.check_expression(&args[2])?; // title (string)
                        self.check_expression(&args[3])?; // monitor
                        self.check_expression(&args[4])?; // share
                        Ok(Type::GLFWwindow)
                    }
                    "glfwWindowShouldClose" => {
                        if args.len() != 1 {
                            bail!("glfwWindowShouldClose() takes 1 argument");
                        }
                        self.check_expression(&args[0])?;
                        Ok(Type::I32)
                    }
                    "glfwPollEvents" => {
                        if args.len() != 0 {
                            bail!("glfwPollEvents() takes no arguments");
                        }
                        Ok(Type::Void)
                    }
                    "glfwGetKey" => {
                        if args.len() != 2 {
                            bail!("glfwGetKey() takes 2 arguments");
                        }
                        self.check_expression(&args[0])?;
                        self.check_expression(&args[1])?;
                        Ok(Type::I32)
                    }
                    "glfwSetWindowShouldClose" => {
                        if args.len() != 2 {
                            bail!("glfwSetWindowShouldClose() takes 2 arguments");
                        }
                        self.check_expression(&args[0])?;
                        self.check_expression(&args[1])?;
                        Ok(Type::Void)
                    }
                    "glfwDestroyWindow" => {
                        if args.len() != 1 {
                            bail!("glfwDestroyWindow() takes 1 argument");
                        }
                        self.check_expression(&args[0])?;
                        Ok(Type::Void)
                    }
                    "glfwTerminate" => {
                        if args.len() != 0 {
                            bail!("glfwTerminate() takes no arguments");
                        }
                        Ok(Type::Void)
                    }
                    "glfwWindowHint" => {
                        if args.len() != 2 {
                            bail!("glfwWindowHint() takes 2 arguments");
                        }
                        self.check_expression(&args[0])?;
                        self.check_expression(&args[1])?;
                        Ok(Type::Void)
                    }
                    _ => Err(anyhow::anyhow!("Not a built-in GLFW function")),
                };
                
                if let Ok(return_type) = glfw_result {
                    return Ok(return_type);
                }
                
                // Handle ImGui built-in functions (basic ones for now)
                let imgui_result = match name.as_str() {
                    "ImGui_Begin" | "ImGui::Begin" => {
                        if args.len() < 1 {
                            bail!("ImGui::Begin() takes at least 1 argument");
                        }
                        self.check_expression(&args[0])?; // title (string)
                        Ok(Type::Bool)
                    }
                    "ImGui_End" | "ImGui::End" => {
                        if args.len() != 0 {
                            bail!("ImGui::End() takes no arguments");
                        }
                        Ok(Type::Void)
                    }
                    "ImGui_Text" | "ImGui::Text" => {
                        if args.len() < 1 {
                            bail!("ImGui::Text() takes at least 1 argument");
                        }
                        for arg in args {
                            self.check_expression(arg)?;
                        }
                        Ok(Type::Void)
                    }
                    "ImGui_Button" | "ImGui::Button" => {
                        if args.len() < 1 {
                            bail!("ImGui::Button() takes at least 1 argument");
                        }
                        self.check_expression(&args[0])?; // label (string)
                        Ok(Type::Bool)
                    }
                    "ImGui_NewFrame" | "ImGui::NewFrame" => {
                        if args.len() != 0 {
                            bail!("ImGui::NewFrame() takes no arguments");
                        }
                        Ok(Type::Void)
                    }
                    "ImGui_Render" | "ImGui::Render" => {
                        if args.len() != 0 {
                            bail!("ImGui::Render() takes no arguments");
                        }
                        Ok(Type::Void)
                    }
                    _ => Err(anyhow::anyhow!("Not a built-in ImGui function")),
                };
                
                if let Ok(return_type) = imgui_result {
                    return Ok(return_type);
                }
                
                // Clone function def to avoid borrow checker issues
                let func = match self.functions.get(name) {
                    Some(f) => f.clone(),
                    None => {
                        // Find similar function names
                        let candidates: Vec<String> = self.functions.keys().cloned().collect();
                        let suggestion = if let Some(closest) = find_closest_match(name, &candidates, 3) {
                            format!("Did you mean '{}'? Use: {}()", closest, closest)
                        } else {
                            format!("Did you mean to declare it? Use: fn {}() {{ ... }}", name)
                        };
                        
                        self.report_error(
                            *location,
                            format!("Undefined function: '{}'", name),
                            Some(suggestion),
                        );
                        // Return Error type instead of bailing - allows error recovery
                        return Ok(Type::Error);
                    }
                };
                
                if args.len() != func.params.len() {
                    self.report_error(
                        *location,
                        format!("Argument count mismatch for function '{}': expected {} arguments, got {}", 
                               name, func.params.len(), args.len()),
                        Some(format!("Call with {} arguments: {}(...)", func.params.len(), name)),
                    );
                    // Return Error type instead of bailing - allows error recovery
                    return Ok(Type::Error);
                }
                
                let mut has_error = false;
                for (i, (arg, param)) in args.iter().zip(func.params.iter()).enumerate() {
                    let arg_type = self.check_expression(arg)?;
                    // If argument is Error type, propagate
                    if matches!(arg_type, Type::Error) {
                        has_error = true;
                        continue;
                    }
                    if !self.types_compatible(&param.ty, &arg_type) {
                        self.report_error(
                            arg.location(),
                            format!("Argument {} type mismatch in function call '{}': expected '{}', got '{}'", 
                                   i + 1, name,
                                   self.type_to_string(&param.ty),
                                   self.type_to_string(&arg_type)),
                            Some(format!("Use a {} value for argument {}", self.type_to_string(&param.ty), i + 1)),
                        );
                        has_error = true;
                    }
                }
                
                if has_error {
                    return Ok(Type::Error);
                }
                
                Ok(func.return_type.clone())
            }
            Expression::MemberAccess { object, member, location } => {
                let object_type = self.check_expression(object)?;
                
                // If object is Error type, propagate
                if matches!(object_type, Type::Error) {
                    return Ok(Type::Error);
                }
                
                // Check if this is unwrap() call on optional type
                if member == "unwrap" {
                    if let Type::Optional(inner_type) = object_type {
                        return Ok(*inner_type);
                    } else {
                        self.report_error(
                            *location,
                            format!("Cannot call unwrap() on non-optional type '{}'", self.type_to_string(&object_type)),
                            Some("unwrap() can only be called on optional types (e.g., ?Type)".to_string()),
                        );
                        // Return Error type instead of bailing - allows error recovery
                        return Ok(Type::Error);
                    }
                }
                
                // For other member access, return placeholder for now
                // TODO: Implement proper member access type checking
                Ok(Type::F32) // Placeholder
            }
            Expression::Index { array, index, location } => {
                let array_type = self.check_expression(array)?;
                let index_type = self.check_expression(index)?;
                
                // If either is Error type, propagate
                if matches!(array_type, Type::Error) || matches!(index_type, Type::Error) {
                    return Ok(Type::Error);
                }
                
                match array_type {
                    Type::Array(element_type) => Ok(*element_type),
                    array_type => {
                        self.report_error(
                            *location,
                            format!("Index operation requires array type, got '{}'", self.type_to_string(&array_type)),
                            Some("Use an array type: array[index]".to_string()),
                        );
                        bail!("Index operation requires array type");
                    }
                }
            }
            Expression::ArrayLiteral { elements, location } => {
                if elements.is_empty() {
                    // Empty array - cannot infer type, require explicit type annotation
                    self.report_error(
                        *location,
                        "Cannot infer type of empty array literal".to_string(),
                        Some("Provide explicit type: let arr: [Type] = [];".to_string()),
                    );
                    // Return Error type instead of bailing - allows error recovery
                    return Ok(Type::Error);
                }
                
                // Infer element type from first element
                let first_type = self.check_expression(&elements[0])?;
                // If first element is Error, propagate
                if matches!(first_type, Type::Error) {
                    return Ok(Type::Error);
                }
                
                let mut has_error = false;
                // Verify all elements have the same type
                for (i, elem) in elements.iter().enumerate().skip(1) {
                    let elem_type = self.check_expression(elem)?;
                    // If element is Error, continue checking others
                    if matches!(elem_type, Type::Error) {
                        has_error = true;
                        continue;
                    }
                    if !self.types_compatible(&first_type, &elem_type) {
                        // Show secondary location pointing to first element for context
                        let first_elem_location = elements[0].location();
                        self.report_error_with_secondary(
                            elem.location(),
                            format!("Array literal element {} has type '{}', but first element has type '{}'", 
                                   i + 1,
                                   self.type_to_string(&elem_type),
                                   self.type_to_string(&first_type)),
                            Some(format!("All array elements must have the same type. Use type '{}' for all elements.", 
                                        self.type_to_string(&first_type))),
                            Some(first_elem_location),
                            Some("Note: first element (expected type)"),
                        );
                        has_error = true;
                    }
                }
                
                if has_error {
                    Ok(Type::Error)
                } else {
                    Ok(Type::Array(Box::new(first_type)))
                }
            }
            Expression::StructLiteral { name, fields: _, location } => {
                // Infer type from struct name
                // Check for built-in struct types first
                match name.as_str() {
                    "Vec2" => Ok(Type::Vec2),
                    "Vec3" => Ok(Type::Vec3),
                    "Vec4" => Ok(Type::Vec4),
                    "Mat4" => Ok(Type::Mat4),
                    _ => {
                        if self.structs.contains_key(name) {
                            Ok(Type::Struct(name.clone()))
                        } else {
                            self.report_error(
                                *location,
                                format!("Undefined struct: '{}'", name),
                                Some(format!("Did you mean to declare it? Use: struct {} {{ ... }}", name)),
                            );
                            Ok(Type::Error)
                        }
                    }
                }
            }
        }
    }
    
    fn types_compatible(&self, expected: &Type, actual: &Type) -> bool {
        // Error type is compatible with everything (allows error recovery)
        if matches!(expected, Type::Error) || matches!(actual, Type::Error) {
            return true;
        }
        
        match (expected, actual) {
            (Type::I32, Type::I32) => true,
            (Type::I64, Type::I64) => true,
            (Type::F32, Type::F32) => true,
            (Type::F64, Type::F64) => true,
            // Implicit numeric conversions (widening and narrowing)
            (Type::I64, Type::I32) => true,  // i32 -> i64 (widening)
            (Type::F64, Type::F32) => true,  // f32 -> f64 (widening)
            (Type::F64, Type::I32) => true,  // i32 -> f64 (widening)
            (Type::F64, Type::I64) => true,  // i64 -> f64 (widening)
            (Type::F32, Type::I32) => true,  // i32 -> f32 (widening)
            (Type::F32, Type::F64) => true,  // f64 -> f32 (narrowing, may lose precision)
            (Type::Bool, Type::Bool) => true,
            (Type::String, Type::String) => true,
            (Type::Void, Type::Void) => true,
            (Type::Array(a), Type::Array(b)) => self.types_compatible(a, b),
            (Type::Optional(a), Type::Optional(b)) => self.types_compatible(a, b),
            // Optional can be assigned from its inner type (implicit wrapping)
            (Type::Optional(inner), actual) => {
                // Allow assigning inner type to optional (implicit wrapping)
                // Also allow null literal (Optional(Void) is a placeholder for null)
                if let Type::Optional(inner_actual) = actual {
                    if matches!(**inner_actual, Type::Void) {
                        true  // null can be assigned to any optional
                    } else {
                        self.types_compatible(inner, actual)
                    }
                } else {
                    self.types_compatible(inner, actual)
                }
            },
            (Type::Struct(a), Type::Struct(b)) => a == b,
            (Type::Component(a), Type::Component(b)) => a == b,
            // Vulkan types
            (Type::VkInstance, Type::VkInstance) => true,
            (Type::VkDevice, Type::VkDevice) => true,
            (Type::VkResult, Type::VkResult) => true,
            (Type::VkPhysicalDevice, Type::VkPhysicalDevice) => true,
            (Type::VkQueue, Type::VkQueue) => true,
            (Type::VkCommandPool, Type::VkCommandPool) => true,
            (Type::VkCommandBuffer, Type::VkCommandBuffer) => true,
            (Type::VkSwapchainKHR, Type::VkSwapchainKHR) => true,
            (Type::VkSurfaceKHR, Type::VkSurfaceKHR) => true,
            (Type::VkRenderPass, Type::VkRenderPass) => true,
            (Type::VkPipeline, Type::VkPipeline) => true,
            (Type::VkFramebuffer, Type::VkFramebuffer) => true,
            (Type::VkBuffer, Type::VkBuffer) => true,
            (Type::VkImage, Type::VkImage) => true,
            (Type::VkImageView, Type::VkImageView) => true,
            (Type::VkSemaphore, Type::VkSemaphore) => true,
            (Type::VkFence, Type::VkFence) => true,
            // GLFW types
            (Type::GLFWwindow, Type::GLFWwindow) => true,
            (Type::GLFWbool, Type::GLFWbool) => true,
            // Math types
            (Type::Vec2, Type::Vec2) => true,
            (Type::Vec3, Type::Vec3) => true,
            (Type::Vec4, Type::Vec4) => true,
            (Type::Mat4, Type::Mat4) => true,
            _ => false,
        }
    }
    
    /// Check if an expression is a frame-scoped allocation (frame.alloc_array call)
    fn is_frame_alloc_expression(&self, expr: &Expression) -> bool {
        match expr {
            Expression::MemberAccess { object, member, .. } => {
                // Check if this is frame.alloc_array
                if member == "alloc_array" {
                    if let Expression::Variable(var_name, ..) = object.as_ref() {
                        return var_name == "frame";
                    }
                }
                false
            }
            Expression::Call { name, .. } => {
                // Check if this is a call to frame.alloc_array (might be parsed as a single call)
                name.contains("alloc_array")
            }
            _ => false,
        }
    }
}
