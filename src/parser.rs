use crate::ast::*;
use crate::lexer::{Token, TokenWithLocation};
use crate::error::{SourceLocation, ErrorReporter};
use anyhow::{Result, bail};

pub struct Parser {
    tokens: Vec<TokenWithLocation>,
    current: usize,
    current_location: SourceLocation,
    error_reporter: Option<ErrorReporter>,
    errors: Vec<(SourceLocation, String, Option<String>)>,  // (location, message, suggestion)
}

impl Parser {
    pub fn new(tokens: Vec<TokenWithLocation>) -> Self {
        let current_location = tokens.first()
            .map(|t| t.location)
            .unwrap_or_else(SourceLocation::unknown);
        Self {
            tokens,
            current: 0,
            current_location,
            error_reporter: None,
            errors: Vec::new(),
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
    
    fn report_parse_error(&mut self, message: String, suggestion: Option<String>) {
        let location = self.current_token_location();
        self.report_error(location, message, suggestion);
    }
    
    pub fn parse(&mut self) -> Result<Program> {
        let mut items = Vec::new();
        
        while !self.is_at_end() {
            items.push(self.parse_item()?);
        }
        
        Ok(Program { items })
    }
    
    fn parse_item(&mut self) -> Result<Item> {
        // Parse attributes first (if any)
        let attrs = self.parse_attributes();
        let is_hot = attrs.contains(&"hot".to_string());
        let is_cuda = attrs.contains(&"cuda".to_string());
        
        match self.peek() {
            Token::Struct => {
                self.advance();
                Ok(Item::Struct(self.parse_struct()?))
            }
            Token::Component => {
                self.advance(); // consume 'component'
                let mut comp = self.parse_component(false, is_hot)?;
                comp.is_cuda = is_cuda;
                Ok(Item::Component(comp))
            }
            Token::ComponentSOA => {
                self.advance(); // consume 'component_soa'
                let mut comp = self.parse_component(true, is_hot)?;
                comp.is_cuda = is_cuda;
                Ok(Item::Component(comp))
            }
            Token::System => {
                self.advance();
                Ok(Item::System(self.parse_system(false)?))
            }
            Token::Shader => {
                self.advance();
                Ok(Item::Shader(self.parse_shader(false)?))
            }
            Token::Hot => {
                // @hot system name { ... } or @hot shader vertex "path" { } or @hot resource Name: Type = "path";
                self.advance();
                if self.check(&Token::System) {
                    self.advance();
                    // Parse system name (might have parentheses for old syntax)
                    let name = if self.check(&Token::LParen) {
                        // Old syntax: system(name) - skip paren and get name
                        self.advance();
                        let name = self.expect_ident()?;
                        self.expect(&Token::RParen)?;
                        name
                    } else {
                        // New syntax: system name
                        self.expect_ident()?
                    };
                    self.expect(&Token::LBrace)?;
                    
                    let mut functions = Vec::new();
                    while !self.check(&Token::RBrace) {
                        if self.check(&Token::Fn) {
                            self.advance();
                            functions.push(self.parse_function()?);
                        } else {
                            let location = self.current_token_location();
                            let suggestion = Some("Add a function declaration: fn function_name() { ... }".to_string());
                            self.report_error(location, "Expected function in system".to_string(), suggestion);
                            bail!("Expected function in system");
                        }
                    }
                    self.expect(&Token::RBrace)?;
                    
                    Ok(Item::System(SystemDef { name, functions, is_hot: true }))
                } else if self.check(&Token::Shader) {
                    self.advance();
                    Ok(Item::Shader(self.parse_shader(true)?))
                } else if self.check(&Token::Component) {
                    self.advance();
                    Ok(Item::Component(self.parse_component(false, true)?))
                } else if self.check(&Token::ComponentSOA) {
                    self.advance();
                    Ok(Item::Component(self.parse_component(true, true)?))
                } else if self.check(&Token::Resource) {
                    self.advance();
                    Ok(Item::Resource(self.parse_resource(true)?))
                } else {
                    let location = self.current_token_location();
                    let suggestion = Some("Use: @hot system Name { ... } or @hot shader vertex \"path\" { }".to_string());
                    self.report_error(location, "Expected 'system', 'shader', 'component', or 'resource' after '@hot'".to_string(), suggestion);
                    bail!("Expected 'system', 'shader', 'component', or 'resource' after '@hot'");
                }
            }
            Token::Extern => {
                self.advance();
                Ok(Item::ExternFunction(self.parse_extern_function()?))
            }
            Token::Fn => {
                self.advance(); // consume 'fn'
                let mut func = self.parse_function()?;
                // Check for @[launch(kernel = name)] in attributes
                for attr in &attrs {
                    if attr.starts_with("launch:") {
                        let kernel_name = attr.strip_prefix("launch:").unwrap().to_string();
                        func.cuda_kernel = Some(kernel_name);
                    }
                }
                Ok(Item::Function(func))
            }
            Token::Resource => {
                self.advance();
                Ok(Item::Resource(self.parse_resource(false)?))
            }
            Token::Pipeline => {
                self.advance();
                Ok(Item::Pipeline(self.parse_pipeline()?))
            }
            _ => {
                let location = self.current_token_location();
                let token_str = format!("{:?}", self.peek());
                let suggestion = Some("Expected: struct, component, system, shader, fn, resource, or pipeline".to_string());
                self.report_error(location, format!("Unexpected token at item level: {}", token_str), suggestion);
                bail!("Unexpected token at item level: {:?}", self.peek());
            }
        }
    }
    
    fn parse_struct(&mut self) -> Result<StructDef> {
        let name = self.expect_ident()?;
        self.expect(&Token::LBrace)?;
        
        let mut fields = Vec::new();
        while !self.check(&Token::RBrace) {
            fields.push(self.parse_field()?);
            if !self.check(&Token::RBrace) {
                self.expect(&Token::Comma)?;
            }
        }
        self.expect(&Token::RBrace)?;
        
        Ok(StructDef { name, fields })
    }
    
    fn parse_attributes(&mut self) -> Vec<String> {
        let mut attrs = Vec::new();
        // Look ahead to see if we have @[ or @hot
        while self.check(&Token::At) {
            self.advance(); // consume '@'
            if self.check(&Token::LBracket) {
                self.advance(); // consume '['
                // Parse attribute name (e.g., "cuda" or "launch")
                if let Token::Ident(ref name) = *self.peek() {
                    let attr_name = name.clone();
                    self.advance();
                    
                    // Check for attribute parameters (e.g., launch(kernel = name))
                    if self.check(&Token::LParen) {
                        self.advance(); // consume '('
                        // Parse parameters (simplified: just look for kernel = name)
                        if let Token::Ident(ref param) = *self.peek() {
                            if param == "kernel" {
                                self.advance(); // consume "kernel"
                                if self.check(&Token::Eq) {
                                    self.advance(); // consume '='
                                    if let Token::Ident(ref kernel_name) = *self.peek() {
                                        let name = kernel_name.clone();
                                        self.advance(); // consume kernel name
                                        attrs.push(format!("launch:{}", name));
                                        self.expect(&Token::RParen).ok(); // consume ')'
                                    }
                                }
                            }
                        }
                    } else {
                        attrs.push(attr_name);
                    }
                    self.expect(&Token::RBracket).ok(); // consume ']'
                }
            } else if self.check(&Token::Hot) {
                // Handle @hot (legacy)
                self.advance();
                attrs.push("hot".to_string());
            } else {
                // Not an attribute, put back the '@'
                self.current -= 1;
                break;
            }
        }
        attrs
    }
    
    fn parse_component(&mut self, is_soa: bool, is_hot: bool) -> Result<ComponentDef> {
        let name = self.expect_ident()?;
        self.expect(&Token::LBrace)?;
        
        let mut fields = Vec::new();
        while !self.check(&Token::RBrace) {
            fields.push(self.parse_field()?);
            if !self.check(&Token::RBrace) {
                self.expect(&Token::Comma)?;
            }
        }
        self.expect(&Token::RBrace)?;
        
        Ok(ComponentDef { name, fields, is_soa, is_hot, is_cuda: false })
    }
    
    fn parse_system(&mut self, is_hot: bool) -> Result<SystemDef> {
        let name = self.expect_ident()?;
        self.expect(&Token::LBrace)?;
        
        let mut functions = Vec::new();
        while !self.check(&Token::RBrace) {
            if self.check(&Token::Fn) {
                self.advance();
                functions.push(self.parse_function()?);
            } else {
                let location = self.current_token_location();
                let suggestion = Some("Add a function declaration: fn function_name() { ... }".to_string());
                self.report_error(location, "Expected function in system".to_string(), suggestion);
                bail!("Expected function in system");
            }
        }
        self.expect(&Token::RBrace)?;
        
        Ok(SystemDef { name, functions, is_hot })
    }
    
    fn parse_shader(&mut self, is_hot: bool) -> Result<crate::ast::ShaderDef> {
        use crate::ast::ShaderStage;
        // Parse shader stage: vertex, fragment, compute, etc.
        let stage = match self.peek() {
            Token::Vertex => {
                self.advance();
                ShaderStage::Vertex
            }
            Token::Fragment => {
                self.advance();
                ShaderStage::Fragment
            }
            Token::Compute => {
                self.advance();
                ShaderStage::Compute
            }
            Token::Geometry => {
                self.advance();
                ShaderStage::Geometry
            }
            Token::TessellationControl => {
                self.advance();
                ShaderStage::TessellationControl
            }
            Token::TessellationEvaluation => {
                self.advance();
                ShaderStage::TessellationEvaluation
            }
            _ => {
                let location = self.current_token_location();
                let suggestion = Some("Use: vertex, fragment, compute, geometry, tessellation_control, or tessellation_evaluation".to_string());
                self.report_error(location, "Expected shader stage (vertex, fragment, compute, etc.)".to_string(), suggestion);
                bail!("Expected shader stage (vertex, fragment, compute, etc.)");
            }
        };
        
        // Parse shader path: "path/to/shader.glsl"
        let path = if let Token::StringLit(ref path) = *self.peek() {
            let path = path.clone();
            self.advance();
            path
        } else {
            let location = self.current_token_location();
            let suggestion = Some("Provide a string literal path: shader vertex \"path/to/shader.vert\"".to_string());
            self.report_error(location, "Expected shader file path string".to_string(), suggestion);
            bail!("Expected shader file path string");
        };
        
        // Parse optional body: { }
        if self.check(&Token::LBrace) {
            self.advance();
            // Skip body content for now (could contain metadata later)
            while !self.check(&Token::RBrace) && !self.is_at_end() {
                self.advance();
            }
            if !self.check(&Token::RBrace) {
                let location = self.current_token_location();
                let suggestion = Some("Add closing brace: }".to_string());
                self.report_error(location, "Expected '}' to close shader declaration".to_string(), suggestion);
                bail!("Expected '}}' to close shader declaration");
            }
            self.advance();
        }
        
        Ok(crate::ast::ShaderDef { stage, path, is_hot })
    }
    
    fn parse_resource(&mut self, is_hot: bool) -> Result<crate::ast::ResourceDef> {
        // Parse: resource Name: Type = "path";
        let name = self.expect_ident()?;
        self.expect(&Token::Colon)?;
        
        // Parse resource type (Texture, Mesh, etc.)
        let resource_type = self.expect_ident()?;
        
        self.expect(&Token::Eq)?;
        
        // Parse file path (string literal)
        let path_token = self.peek().clone();
        let path = match path_token {
            Token::StringLit(p) => {
                self.advance();
                p
            }
            _ => {
                let location = self.current_token_location();
                let suggestion = Some("Provide a string literal path: resource Name: Type = \"path/to/file\"".to_string());
                self.report_error(location, format!("Expected string literal for resource path, got: {:?}", path_token), suggestion);
                bail!("Expected string literal for resource path, got: {:?}", path_token);
            }
        };
        
        self.expect(&Token::Semicolon)?;
        
        Ok(crate::ast::ResourceDef {
            name,
            resource_type,
            path,
            is_hot,
        })
    }
    
    fn parse_pipeline(&mut self) -> Result<crate::ast::PipelineDef> {
        use crate::ast::{PipelineDef, PipelineShader, PipelineLayout, LayoutBinding, BindingType, ShaderStage};
        
        // Parse: pipeline name { shader vertex "path"; shader fragment "path"; layout { ... } }
        let name = self.expect_ident()?;
        self.expect(&Token::LBrace)?;
        
        let mut shaders = Vec::new();
        let mut layout = None;
        
        while !self.check(&Token::RBrace) {
            if self.check(&Token::Shader) {
                self.advance();
                
                // Parse shader stage
                let stage = match self.peek() {
                    Token::Vertex => {
                        self.advance();
                        ShaderStage::Vertex
                    }
                    Token::Fragment => {
                        self.advance();
                        ShaderStage::Fragment
                    }
                    Token::Compute => {
                        self.advance();
                        ShaderStage::Compute
                    }
                    Token::Geometry => {
                        self.advance();
                        ShaderStage::Geometry
                    }
                    Token::TessellationControl => {
                        self.advance();
                        ShaderStage::TessellationControl
                    }
                    Token::TessellationEvaluation => {
                        self.advance();
                        ShaderStage::TessellationEvaluation
                    }
                    _ => {
                        let location = self.current_token_location();
                        let suggestion = Some("Use: vertex, fragment, compute, geometry, tessellation_control, or tessellation_evaluation".to_string());
                        self.report_error(location, "Expected shader stage (vertex, fragment, compute, etc.)".to_string(), suggestion);
                        bail!("Expected shader stage (vertex, fragment, compute, etc.)");
                    }
                };
                
                // Parse shader path
                let path_token = self.peek().clone();
                let path = match path_token {
                    Token::StringLit(p) => {
                        self.advance();
                        p
                    }
                    _ => {
                        let location = self.current_token_location();
                        let suggestion = Some("Provide a string literal path: shader vertex \"path/to/shader.vert\"".to_string());
                        self.report_error(location, "Expected string literal for shader path".to_string(), suggestion);
                        bail!("Expected string literal for shader path");
                    }
                };
                
                shaders.push(PipelineShader { stage, path });
            } else if self.check(&Token::Layout) {
                self.advance();
                self.expect(&Token::LBrace)?;
                
                let mut bindings = Vec::new();
                while !self.check(&Token::RBrace) {
                    // Parse: binding N: type ResourceName
                    self.expect(&Token::Binding)?;
                    let binding_num_token = self.peek().clone();
                    let binding_num = match binding_num_token {
                        Token::Int(n) => {
                            self.advance();
                            n as u32
                        }
                        _ => {
                            let location = self.current_token_location();
                            let suggestion = Some("Provide a binding number: binding 0: uniform TypeName".to_string());
                            self.report_error(location, "Expected binding number".to_string(), suggestion);
                            bail!("Expected binding number");
                        }
                    };
                    self.expect(&Token::Colon)?;
                    
                    // Parse binding type
                    let binding_type = if self.check(&Token::Uniform) {
                        self.advance();
                        let type_name = self.expect_ident()?;
                        BindingType::Uniform(type_name)
                    } else if self.check(&Token::Storage) {
                        self.advance();
                        let type_name = self.expect_ident()?;
                        // Check for array syntax []
                        if self.check(&Token::LBracket) {
                            self.advance();
                            self.expect(&Token::RBracket)?;
                        }
                        BindingType::Storage(type_name)
                    } else if self.check(&Token::Sampler2D) {
                        self.advance();
                        // Check for array syntax []
                        if self.check(&Token::LBracket) {
                            self.advance();
                            self.expect(&Token::RBracket)?;
                        }
                        BindingType::Sampler2D
                    } else {
                        let location = self.current_token_location();
                        let suggestion = Some("Use: uniform TypeName, storage TypeName[], or sampler2D".to_string());
                        self.report_error(location, "Expected binding type: uniform, storage, or sampler2D".to_string(), suggestion);
                        bail!("Expected binding type: uniform, storage, or sampler2D");
                    };
                    
                    // Parse resource name (optional, for reference)
                    let resource_name = if matches!(self.peek(), Token::Ident(_)) {
                        let name = self.expect_ident()?;
                        name
                    } else {
                        String::new()
                    };
                    
                    bindings.push(LayoutBinding {
                        binding: binding_num,
                        binding_type,
                        name: resource_name,
                    });
                    
                    if !self.check(&Token::RBrace) {
                        // Optional comma or semicolon
                        if self.check(&Token::Comma) {
                            self.advance();
                        } else if self.check(&Token::Semicolon) {
                            self.advance();
                        }
                    }
                }
                self.expect(&Token::RBrace)?;
                
                // Validate bindings: check for duplicate binding indices
                let mut seen_bindings = std::collections::HashSet::new();
                for binding in &bindings {
                    if !seen_bindings.insert(binding.binding) {
                        let location = self.current_token_location();
                        let suggestion = Some(format!("Use a different binding index. Binding {} is already used.", binding.binding));
                        self.report_error(location, format!("Duplicate binding index {} in pipeline '{}' layout", binding.binding, name), suggestion);
                        bail!("Duplicate binding index {} in pipeline '{}' layout", binding.binding, name);
                    }
                }
                
                layout = Some(PipelineLayout { bindings });
            } else {
                let location = self.current_token_location();
                let suggestion = Some("Use: shader vertex \"path\" or layout { binding ... }".to_string());
                self.report_error(location, "Expected 'shader' or 'layout' in pipeline declaration".to_string(), suggestion);
                bail!("Expected 'shader' or 'layout' in pipeline declaration");
            }
        }
        
        self.expect(&Token::RBrace)?;
        
        Ok(PipelineDef { name, shaders, layout })
    }
    
    fn parse_extern_function(&mut self) -> Result<ExternFunctionDef> {
        self.expect(&Token::Fn)?;
        let name = self.expect_ident()?;
        self.expect(&Token::LParen)?;
        
        let mut params = Vec::new();
        if !self.check(&Token::RParen) {
            loop {
                let param_name = self.expect_ident()?;
                self.expect(&Token::Colon)?;
                let param_type = self.parse_type()?;
                params.push(Param {
                    name: param_name,
                    ty: param_type,
                });
                
                if !self.check(&Token::Comma) {
                    break;
                }
                self.advance();
            }
        }
        self.expect(&Token::RParen)?;
        
        let return_type = if self.check(&Token::Colon) {
            self.advance();
            self.parse_type()?
        } else {
            Type::Void
        };
        
        // Optional library name: extern fn name() from "library"
        let library = if let Token::Ident(ref s) = *self.peek() {
            if s == "from" {
                self.advance(); // "from"
                let lib_token = self.peek().clone();
                if let Token::StringLit(lib_name) = lib_token {
                    self.advance();
                    Some(lib_name)
                } else {
                    None
                }
            } else {
                None
            }
        } else {
            None
        };
        
        self.expect(&Token::Semicolon)?;
        
        Ok(ExternFunctionDef {
            name,
            params,
            return_type,
            library,
        })
    }
    
    fn parse_function(&mut self) -> Result<FunctionDef> {
        let name = self.expect_ident()?;
        self.expect(&Token::LParen)?;
        
        let mut params = Vec::new();
        if !self.check(&Token::RParen) {
            loop {
                let param_name = self.expect_ident()?;
                self.expect(&Token::Colon)?;
                let param_type = self.parse_type()?;
                params.push(Param {
                    name: param_name,
                    ty: param_type,
                });
                
                if !self.check(&Token::Comma) {
                    break;
                }
                self.advance();
            }
        }
        self.expect(&Token::RParen)?;
        
        let return_type = if self.check(&Token::Colon) {
            self.advance();
            self.parse_type()?
        } else {
            Type::Void
        };
        
        let body = self.parse_block()?;
        
        Ok(FunctionDef {
            name,
            params,
            return_type,
            body,
            cuda_kernel: None,  // Will be set by caller if @[launch] attribute present
        })
    }
    
    fn parse_field(&mut self) -> Result<Field> {
        let name = self.expect_ident()?;
        self.expect(&Token::Colon)?;
        let ty = self.parse_type()?;
        Ok(Field { name, ty })
    }
    
    fn parse_type(&mut self) -> Result<Type> {
        match self.peek() {
            Token::I32 => {
                self.advance();
                Ok(Type::I32)
            }
            Token::I64 => {
                self.advance();
                Ok(Type::I64)
            }
            Token::F32 => {
                self.advance();
                Ok(Type::F32)
            }
            Token::F64 => {
                self.advance();
                Ok(Type::F64)
            }
            Token::Bool => {
                self.advance();
                Ok(Type::Bool)
            }
            Token::String => {
                self.advance();
                Ok(Type::String)
            }
            Token::Void => {
                self.advance();
                Ok(Type::Void)
            }
            Token::VkInstance => {
                self.advance();
                Ok(Type::VkInstance)
            }
            Token::VkDevice => {
                self.advance();
                Ok(Type::VkDevice)
            }
            Token::VkResult => {
                self.advance();
                Ok(Type::VkResult)
            }
            Token::VkPhysicalDevice => {
                self.advance();
                Ok(Type::VkPhysicalDevice)
            }
            Token::VkQueue => {
                self.advance();
                Ok(Type::VkQueue)
            }
            Token::VkCommandPool => {
                self.advance();
                Ok(Type::VkCommandPool)
            }
            Token::VkCommandBuffer => {
                self.advance();
                Ok(Type::VkCommandBuffer)
            }
            Token::VkSwapchainKHR => {
                self.advance();
                Ok(Type::VkSwapchainKHR)
            }
            Token::VkSurfaceKHR => {
                self.advance();
                Ok(Type::VkSurfaceKHR)
            }
            Token::VkRenderPass => {
                self.advance();
                Ok(Type::VkRenderPass)
            }
            Token::VkPipeline => {
                self.advance();
                Ok(Type::VkPipeline)
            }
            Token::VkFramebuffer => {
                self.advance();
                Ok(Type::VkFramebuffer)
            }
            Token::VkBuffer => {
                self.advance();
                Ok(Type::VkBuffer)
            }
            Token::VkImage => {
                self.advance();
                Ok(Type::VkImage)
            }
            Token::VkImageView => {
                self.advance();
                Ok(Type::VkImageView)
            }
            Token::VkSemaphore => {
                self.advance();
                Ok(Type::VkSemaphore)
            }
            Token::VkFence => {
                self.advance();
                Ok(Type::VkFence)
            }
            Token::GLFWwindow => {
                self.advance();
                Ok(Type::GLFWwindow)
            }
            Token::GLFWbool => {
                self.advance();
                Ok(Type::GLFWbool)
            }
            Token::Vec2 => {
                self.advance();
                Ok(Type::Vec2)
            }
            Token::Vec3 => {
                self.advance();
                Ok(Type::Vec3)
            }
            Token::Vec4 => {
                self.advance();
                Ok(Type::Vec4)
            }
            Token::Mat4 => {
                self.advance();
                Ok(Type::Mat4)
            }
            Token::Query => {
                // Parse query<Component1, Component2, ...>
                self.advance();
                self.expect(&Token::Lt)?;
                let mut component_types = Vec::new();
                loop {
                    let ty = self.parse_type()?;
                    component_types.push(ty);
                    if self.check(&Token::Comma) {
                        self.advance();
                    } else {
                        break;
                    }
                }
                self.expect(&Token::Gt)?;
                Ok(Type::Query(component_types))
            }
            Token::Ident(ref name) => {
                let name_clone = name.clone();
                self.advance();
                Ok(Type::Struct(name_clone))
            }
            Token::LBracket => {
                self.advance();
                let element_type = self.parse_type()?;
                self.expect(&Token::RBracket)?;
                Ok(Type::Array(Box::new(element_type)))
            }
            Token::Question => {
                // Parse optional type: ?Type
                self.advance();
                let inner_type = self.parse_type()?;
                Ok(Type::Optional(Box::new(inner_type)))
            }
            _ => {
                let location = self.current_token_location();
                let token_str = format!("{:?}", self.peek());
                let suggestion = Some("Expected: i32, i64, f32, f64, bool, string, void, or a type name".to_string());
                self.report_error(location, format!("Unexpected token in type: {}", token_str), suggestion);
                bail!("Unexpected token in type: {:?}", self.peek());
            }
        }
    }
    
    fn parse_block(&mut self) -> Result<Vec<Statement>> {
        self.expect(&Token::LBrace)?;
        let mut statements = Vec::new();
        
        while !self.check(&Token::RBrace) {
            statements.push(self.parse_statement()?);
        }
        self.expect(&Token::RBrace)?;
        
        Ok(statements)
    }
    
    // Helper to create a Block statement with location (used internally)
    fn create_block_statement(&self, statements: Vec<Statement>, location: SourceLocation) -> Statement {
        Statement::Block(statements, location)
    }
    
    fn parse_statement(&mut self) -> Result<Statement> {
        let stmt_location = self.current_token_location();
        match self.peek() {
            Token::Let => {
                self.advance();
                let name = self.expect_ident()?;
                let ty = if self.check(&Token::Colon) {
                    self.advance();
                    Some(self.parse_type()?)
                } else {
                    None
                };
                self.expect(&Token::Eq)?;
                let value = self.parse_expression()?;
                self.expect(&Token::Semicolon)?;
                Ok(Statement::Let { name, ty, value, location: stmt_location })
            }
            Token::If => {
                self.advance();
                // Optional parentheses around condition
                let condition = if self.check(&Token::LParen) {
                    self.advance();
                    let expr = self.parse_expression()?;
                    self.expect(&Token::RParen)?;
                    expr
                } else {
                    self.parse_expression()?
                };
                let then_block = self.parse_block()?;
                let else_block = if self.check(&Token::Else) {
                    self.advance();
                    Some(self.parse_block()?)
                } else {
                    None
                };
                Ok(Statement::If {
                    condition,
                    then_block,
                    else_block,
                    location: stmt_location,
                })
            }
            Token::While => {
                self.advance();
                // Optional parentheses around condition
                let condition = if self.check(&Token::LParen) {
                    self.advance();
                    let expr = self.parse_expression()?;
                    self.expect(&Token::RParen)?;
                    expr
                } else {
                    self.parse_expression()?
                };
                let body = self.parse_block()?;
                Ok(Statement::While { condition, body, location: stmt_location })
            }
            Token::For => {
                // Parse: for <iterator> in <collection> { ... }
                self.advance();
                let iterator = self.expect_ident()?;
                self.expect(&Token::In)?;
                let collection = self.parse_expression()?;
                let body = self.parse_block()?;
                Ok(Statement::For { iterator, collection, body, location: stmt_location })
            }
            Token::Loop => {
                self.advance();
                let body = self.parse_block()?;
                Ok(Statement::Loop { body, location: stmt_location })
            }
            Token::Return => {
                self.advance();
                let expr = if !self.check(&Token::Semicolon) {
                    Some(self.parse_expression()?)
                } else {
                    None
                };
                self.expect(&Token::Semicolon)?;
                Ok(Statement::Return(expr, stmt_location))
            }
            Token::Defer => {
                self.advance();
                let expr = self.parse_expression()?;
                self.expect(&Token::Semicolon)?;
                Ok(Statement::Defer(Box::new(expr), stmt_location))
            }
            _ => {
                let expr = self.parse_expression()?;
                if self.check(&Token::Eq) {
                    self.advance();
                    let value = self.parse_expression()?;
                    self.expect(&Token::Semicolon)?;
                    Ok(Statement::Assign {
                        target: expr,
                        value,
                        location: stmt_location,
                    })
                } else {
                    self.expect(&Token::Semicolon)?;
                    Ok(Statement::Expression(expr, stmt_location))
                }
            }
        }
    }
    
    fn parse_expression(&mut self) -> Result<Expression> {
        self.parse_assignment()
    }
    
    fn parse_assignment(&mut self) -> Result<Expression> {
        let expr = self.parse_or()?;
        Ok(expr)
    }
    
    fn parse_or(&mut self) -> Result<Expression> {
        let mut expr = self.parse_and()?;
        
        while self.check(&Token::OrOr) {
            let location = self.current_token_location();
            self.advance();
            let right = self.parse_and()?;
            expr = Expression::BinaryOp {
                op: BinaryOp::Or,
                left: Box::new(expr),
                right: Box::new(right),
                location,
            };
        }
        
        Ok(expr)
    }
    
    fn parse_and(&mut self) -> Result<Expression> {
        let mut expr = self.parse_equality()?;
        
        while self.check(&Token::AndAnd) {
            let location = self.current_token_location();
            self.advance();
            let right = self.parse_equality()?;
            expr = Expression::BinaryOp {
                op: BinaryOp::And,
                left: Box::new(expr),
                right: Box::new(right),
                location,
            };
        }
        
        Ok(expr)
    }
    
    fn parse_equality(&mut self) -> Result<Expression> {
        let mut expr = self.parse_comparison()?;
        
        while self.check(&Token::EqEq) || self.check(&Token::Ne) {
            let location = self.current_token_location();
            let op = if self.check(&Token::EqEq) {
                self.advance();
                BinaryOp::Eq
            } else {
                self.advance();
                BinaryOp::Ne
            };
            let right = self.parse_comparison()?;
            expr = Expression::BinaryOp {
                op,
                left: Box::new(expr),
                right: Box::new(right),
                location,
            };
        }
        
        Ok(expr)
    }
    
    fn parse_comparison(&mut self) -> Result<Expression> {
        let mut expr = self.parse_term()?;
        
        while matches!(self.peek(), Token::Lt | Token::Le | Token::Gt | Token::Ge) {
            let location = self.current_token_location();
            let op = match self.peek() {
                Token::Lt => {
                    self.advance();
                    BinaryOp::Lt
                }
                Token::Le => {
                    self.advance();
                    BinaryOp::Le
                }
                Token::Gt => {
                    self.advance();
                    BinaryOp::Gt
                }
                Token::Ge => {
                    self.advance();
                    BinaryOp::Ge
                }
                _ => unreachable!(),
            };
            let right = self.parse_term()?;
            expr = Expression::BinaryOp {
                op,
                left: Box::new(expr),
                right: Box::new(right),
                location,
            };
        }
        
        Ok(expr)
    }
    
    fn parse_term(&mut self) -> Result<Expression> {
        let mut expr = self.parse_factor()?;
        
        while self.check(&Token::Plus) || self.check(&Token::Minus) {
            let location = self.current_token_location();
            let op = if self.check(&Token::Plus) {
                self.advance();
                BinaryOp::Add
            } else {
                self.advance();
                BinaryOp::Sub
            };
            let right = self.parse_factor()?;
            expr = Expression::BinaryOp {
                op,
                left: Box::new(expr),
                right: Box::new(right),
                location,
            };
        }
        
        Ok(expr)
    }
    
    fn parse_factor(&mut self) -> Result<Expression> {
        let mut expr = self.parse_unary()?;
        
        while self.check(&Token::Star) || self.check(&Token::Slash) || self.check(&Token::Percent) {
            let location = self.current_token_location();
            let op = match self.peek() {
                Token::Star => {
                    self.advance();
                    BinaryOp::Mul
                }
                Token::Slash => {
                    self.advance();
                    BinaryOp::Div
                }
                Token::Percent => {
                    self.advance();
                    BinaryOp::Mod
                }
                _ => unreachable!(),
            };
            let right = self.parse_unary()?;
            expr = Expression::BinaryOp {
                op,
                left: Box::new(expr),
                right: Box::new(right),
                location,
            };
        }
        
        Ok(expr)
    }
    
    fn parse_unary(&mut self) -> Result<Expression> {
        if self.check(&Token::Bang) {
            let location = self.current_token_location();
            self.advance();
            let expr = self.parse_unary()?;
            return Ok(Expression::UnaryOp {
                op: UnaryOp::Not,
                expr: Box::new(expr),
                location,
            });
        }
        
        if self.check(&Token::Minus) {
            let location = self.current_token_location();
            self.advance();
            let expr = self.parse_unary()?;
            return Ok(Expression::UnaryOp {
                op: UnaryOp::Neg,
                expr: Box::new(expr),
                location,
            });
        }
        
        self.parse_call()
    }
    
    fn parse_call(&mut self) -> Result<Expression> {
        let mut expr = self.parse_primary()?;
        
        loop {
            if self.check(&Token::LParen) {
                self.advance();
                let mut args = Vec::new();
                if !self.check(&Token::RParen) {
                    loop {
                        args.push(self.parse_expression()?);
                        if !self.check(&Token::Comma) {
                            break;
                        }
                        self.advance();
                    }
                }
                self.expect(&Token::RParen)?;
                
                if let Expression::Variable(name, _var_location) = expr {
                    let call_location = self.current_token_location();
                    // Check if this is a struct constructor (Vec2, Vec3, Vec4)
                    match name.as_str() {
                        "Vec2" => {
                            if args.len() != 2 {
                                self.report_error(call_location, format!("Vec2 constructor expects 2 arguments, got {}", args.len()), Some("Use: Vec2(x, y)".to_string()));
                                bail!("Vec2 constructor expects 2 arguments");
                            }
                            expr = Expression::StructLiteral {
                                name: "Vec2".to_string(),
                                fields: vec![
                                    ("x".to_string(), args[0].clone()),
                                    ("y".to_string(), args[1].clone()),
                                ],
                                location: call_location,
                            };
                        }
                        "Vec3" => {
                            if args.len() != 3 {
                                self.report_error(call_location, format!("Vec3 constructor expects 3 arguments, got {}", args.len()), Some("Use: Vec3(x, y, z)".to_string()));
                                bail!("Vec3 constructor expects 3 arguments");
                            }
                            expr = Expression::StructLiteral {
                                name: "Vec3".to_string(),
                                fields: vec![
                                    ("x".to_string(), args[0].clone()),
                                    ("y".to_string(), args[1].clone()),
                                    ("z".to_string(), args[2].clone()),
                                ],
                                location: call_location,
                            };
                        }
                        "Vec4" => {
                            if args.len() != 4 {
                                self.report_error(call_location, format!("Vec4 constructor expects 4 arguments, got {}", args.len()), Some("Use: Vec4(x, y, z, w)".to_string()));
                                bail!("Vec4 constructor expects 4 arguments");
                            }
                            expr = Expression::StructLiteral {
                                name: "Vec4".to_string(),
                                fields: vec![
                                    ("x".to_string(), args[0].clone()),
                                    ("y".to_string(), args[1].clone()),
                                    ("z".to_string(), args[2].clone()),
                                    ("w".to_string(), args[3].clone()),
                                ],
                                location: call_location,
                            };
                        }
                        _ => {
                            // Regular function call
                            expr = Expression::Call { name, args, location: call_location };
                        }
                    }
                } else {
                    let location = self.current_token_location();
                    let suggestion = Some("Use an identifier for the function name: function_name(...)".to_string());
                    self.report_error(location, "Expected function name".to_string(), suggestion);
                    bail!("Expected function name");
                }
            } else if self.check(&Token::Dot) {
                let dot_location = self.current_token_location();
                self.advance();
                let member = self.expect_ident()?;
                expr = Expression::MemberAccess {
                    object: Box::new(expr),
                    member,
                    location: dot_location,
                };
            } else if self.check(&Token::LBracket) {
                let bracket_location = self.current_token_location();
                self.advance();
                let index = self.parse_expression()?;
                self.expect(&Token::RBracket)?;
                expr = Expression::Index {
                    array: Box::new(expr),
                    index: Box::new(index),
                    location: bracket_location,
                };
            } else {
                break;
            }
        }
        
        Ok(expr)
    }
    
    fn parse_primary(&mut self) -> Result<Expression> {
        let location = self.current_token_location();
        let token = self.peek().clone();
        match token {
            Token::Int(n) => {
                self.advance();
                Ok(Expression::Literal(Literal::Int(n), location))
            }
            Token::Float(n) => {
                self.advance();
                Ok(Expression::Literal(Literal::Float(n), location))
            }
            Token::True => {
                self.advance();
                Ok(Expression::Literal(Literal::Bool(true), location))
            }
            Token::False => {
                self.advance();
                Ok(Expression::Literal(Literal::Bool(false), location))
            }
            Token::Null => {
                self.advance();
                // Null literal - return Optional(Void) type placeholder
                // This will be handled in type checking
                bail!("Null literal not yet fully supported - use Optional types");
            }
            Token::StringLit(s) => {
                self.advance();
                // Check if string contains interpolation syntax: {variable}
                if s.contains('{') && s.contains('}') {
                    // Parse string interpolation
                    self.parse_string_interpolation(&s, location)
                } else {
                    Ok(Expression::Literal(Literal::String(s), location))
                }
            }
            Token::Ident(name) => {
                self.advance();
                Ok(Expression::Variable(name, location))
            }
            Token::Vec2 => {
                self.advance();
                Ok(Expression::Variable("Vec2".to_string(), location))
            }
            Token::Vec3 => {
                self.advance();
                Ok(Expression::Variable("Vec3".to_string(), location))
            }
            Token::Vec4 => {
                self.advance();
                Ok(Expression::Variable("Vec4".to_string(), location))
            }
            Token::Mat4 => {
                self.advance();
                Ok(Expression::Variable("Mat4".to_string(), location))
            }
            Token::LParen => {
                self.advance();
                let expr = self.parse_expression()?;
                self.expect(&Token::RParen)?;
                Ok(expr)
            }
            Token::LBracket => {
                // Parse array literal: [expr1, expr2, ...]
                let array_location = self.current_token_location();
                self.advance();
                let mut elements = Vec::new();
                
                if !self.check(&Token::RBracket) {
                    loop {
                        elements.push(self.parse_expression()?);
                        if !self.check(&Token::Comma) {
                            break;
                        }
                        self.advance();
                    }
                }
                
                self.expect(&Token::RBracket)?;
                Ok(Expression::ArrayLiteral { elements, location: array_location })
            }
            Token::Match => {
                self.parse_match_expression()
            }
            _ => {
                let location = self.current_token_location();
                let token_str = format!("{:?}", self.peek());
                let suggestion = Some("Expected: literal, identifier, or expression".to_string());
                self.report_error(location, format!("Unexpected token in expression: {}", token_str), suggestion);
                bail!("Unexpected token in expression: {:?}", self.peek());
            }
        }
    }
    
    fn parse_match_expression(&mut self) -> Result<Expression> {
        use crate::ast::{MatchArm, Expression};
        let match_location = self.current_token_location();
        self.advance(); // consume 'match'
        
        // Parse the expression being matched
        let expr = self.parse_expression()?;
        
        // Parse the match body: { pattern => { ... }, pattern => { ... } }
        self.expect(&Token::LBrace)?;
        
        let mut arms = Vec::new();
        while !self.check(&Token::RBrace) {
            let arm_location = self.current_token_location();
            
            // Parse pattern
            let pattern = self.parse_pattern()?;
            
            // Expect => arrow (can be = followed by >, or a single => token if we add it)
            // For now, parse = followed by >
            if !self.check(&Token::Eq) {
                let suggestion = Some("Use: pattern => { body }".to_string());
                self.report_error(arm_location, "Expected '=>' after pattern".to_string(), suggestion);
                bail!("Expected '=>' after pattern at {:?}", arm_location);
            }
            self.advance(); // consume '='
            if !self.check(&Token::Gt) {
                let suggestion = Some("Use: pattern => { body } (the => arrow)".to_string());
                self.report_error(arm_location, "Expected '>' after '=' in '=>'".to_string(), suggestion);
                bail!("Expected '>' after '=' in '=>' at {:?}", arm_location);
            }
            self.advance(); // consume '>'
            
            // Parse body (block of statements)
            let body = self.parse_block()?;
            
            arms.push(MatchArm { pattern, body, location: arm_location });
            
            // Optional comma between arms
            if self.check(&Token::Comma) {
                self.advance();
            }
        }
        
        self.expect(&Token::RBrace)?;
        
        Ok(Expression::Match { expr: Box::new(expr), arms, location: match_location })
    }
    
    fn parse_pattern(&mut self) -> Result<Pattern> {
        use crate::ast::{Pattern, Literal};
        let pattern_location = self.current_token_location();
        let token = self.peek().clone();
        
        match token {
            Token::Int(n) => {
                self.advance();
                Ok(Pattern::Literal(Literal::Int(n), pattern_location))
            }
            Token::Float(n) => {
                self.advance();
                Ok(Pattern::Literal(Literal::Float(n), pattern_location))
            }
            Token::True => {
                self.advance();
                Ok(Pattern::Literal(Literal::Bool(true), pattern_location))
            }
            Token::False => {
                self.advance();
                Ok(Pattern::Literal(Literal::Bool(false), pattern_location))
            }
            Token::StringLit(s) => {
                self.advance();
                Ok(Pattern::Literal(Literal::String(s), pattern_location))
            }
            Token::Ident(name) => {
                self.advance();
                // Check if it's a wildcard
                if name == "_" {
                    Ok(Pattern::Wildcard(pattern_location))
                } else {
                    // For now, treat all identifiers as variable bindings
                    // This allows: match x { value => { ... } }
                    // TODO: Distinguish between variable bindings and enum variants/constants
                    Ok(Pattern::Variable(name, pattern_location))
                }
            }
            _ => {
                let suggestion = Some("Expected: literal, identifier, or wildcard (_)".to_string());
                self.report_error(pattern_location, format!("Unexpected token in pattern: {:?}", token), suggestion);
                bail!("Unexpected token in pattern: {:?}", token);
            }
        }
    }
    
    fn expect_ident(&mut self) -> Result<String> {
        let token = self.peek().clone();
        match token {
            Token::Ident(name) => {
                self.advance();
                Ok(name)
            }
            _ => {
                let location = self.current_token_location();
                let token_str = format!("{:?}", self.peek());
                let suggestion = Some("Use an identifier: a name starting with a letter or underscore".to_string());
                self.report_error(location, format!("Expected identifier, got {}", token_str), suggestion);
                bail!("Expected identifier, got {:?}", self.peek());
            }
        }
    }
    
    fn expect(&mut self, token: &Token) -> Result<()> {
        if self.check(token) {
            self.advance();
            Ok(())
        } else {
            let location = self.current_token_location();
            let suggestion = Some(format!("Expected token: {:?}", token));
            self.report_error(location, format!("Expected {:?}, got {:?}", token, self.peek()), suggestion);
            bail!("Expected {:?}, got {:?}", token, self.peek())
        }
    }
    
    fn check(&self, token: &Token) -> bool {
        !self.is_at_end() && std::mem::discriminant(self.peek()) == std::mem::discriminant(token)
    }
    
    fn peek(&self) -> &Token {
        &self.tokens[self.current].token
    }
    
    fn advance(&mut self) {
        if !self.is_at_end() {
            self.current_location = self.tokens[self.current].location;
            self.current += 1;
        }
    }
    
    fn current_token_location(&self) -> SourceLocation {
        if self.current < self.tokens.len() {
            self.tokens[self.current].location
        } else {
            self.current_location
        }
    }
    
    fn is_at_end(&self) -> bool {
        self.current >= self.tokens.len()
    }
    
    fn parse_string_interpolation(&mut self, s: &str, location: SourceLocation) -> Result<Expression> {
        use crate::ast::{StringInterpolationPart, Expression, Literal};
        
        let mut parts = Vec::new();
        let mut current_literal = String::new();
        let mut chars = s.chars().peekable();
        
        while let Some(ch) = chars.next() {
            if ch == '{' {
                // Save current literal if any
                if !current_literal.is_empty() {
                    parts.push(StringInterpolationPart::Literal(current_literal.clone()));
                    current_literal.clear();
                }
                
                // Parse variable name inside {}
                let mut var_name = String::new();
                let mut found_closing = false;
                
                while let Some(&next_ch) = chars.peek() {
                    if next_ch == '}' {
                        chars.next(); // consume '}'
                        found_closing = true;
                        break;
                    } else if next_ch.is_alphanumeric() || next_ch == '_' {
                        var_name.push(chars.next().unwrap());
                    } else {
                        // Invalid character in interpolation
                        bail!("Invalid character in string interpolation variable name: '{}' at {:?}", next_ch, location);
                    }
                }
                
                if !found_closing {
                    let suggestion = Some("Close the interpolation: \"text {variable}\"".to_string());
                    self.report_error(location, "Unclosed string interpolation brace".to_string(), suggestion);
                    bail!("Unclosed string interpolation brace at {:?}", location);
                }
                
                if var_name.is_empty() {
                    let suggestion = Some("Provide a variable name: \"text {variable_name}\"".to_string());
                    self.report_error(location, "Empty variable name in string interpolation".to_string(), suggestion);
                    bail!("Empty variable name in string interpolation at {:?}", location);
                }
                
                parts.push(StringInterpolationPart::Variable(var_name));
            } else if ch == '}' {
                // Unmatched closing brace
                let suggestion = Some("Remove the extra '}' or add a matching '{'".to_string());
                self.report_error(location, "Unmatched closing brace in string interpolation".to_string(), suggestion);
                bail!("Unmatched closing brace in string interpolation at {:?}", location);
            } else {
                current_literal.push(ch);
            }
        }
        
        // Add remaining literal if any
        if !current_literal.is_empty() {
            parts.push(StringInterpolationPart::Literal(current_literal));
        }
        
        // If no interpolation parts (shouldn't happen, but handle gracefully)
        if parts.is_empty() {
            return Ok(Expression::Literal(Literal::String(s.to_string()), location));
        }
        
        // If only one literal part, return as regular string literal
        if parts.len() == 1 {
            if let StringInterpolationPart::Literal(lit) = &parts[0] {
                return Ok(Expression::Literal(Literal::String(lit.clone()), location));
            }
        }
        
        Ok(Expression::StringInterpolation { parts, location })
    }
}

