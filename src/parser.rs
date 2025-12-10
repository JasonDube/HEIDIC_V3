use crate::ast::*;
use crate::lexer::{Token, TokenWithLocation};
use crate::error::SourceLocation;
use anyhow::{Result, bail};

pub struct Parser {
    tokens: Vec<TokenWithLocation>,
    current: usize,
    current_location: SourceLocation,
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
        }
    }
    
    // Legacy constructor for backward compatibility
    pub fn new_simple(tokens: Vec<Token>) -> Self {
        let tokens_with_location: Vec<TokenWithLocation> = tokens.into_iter()
            .map(|token| TokenWithLocation {
                token,
                location: SourceLocation::unknown(),
            })
            .collect();
        Self::new(tokens_with_location)
    }
    
    pub fn parse(&mut self) -> Result<Program> {
        let mut items = Vec::new();
        
        while !self.is_at_end() {
            items.push(self.parse_item()?);
        }
        
        Ok(Program { items })
    }
    
    fn parse_item(&mut self) -> Result<Item> {
        match self.peek() {
            Token::Struct => {
                self.advance();
                Ok(Item::Struct(self.parse_struct()?))
            }
            Token::Component => {
                self.advance();
                Ok(Item::Component(self.parse_component(false)?))
            }
            Token::ComponentSOA => {
                self.advance();
                Ok(Item::Component(self.parse_component(true)?))
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
                // @hot system name { ... } or @hot shader vertex "path" { }
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
                            bail!("Expected function in system");
                        }
                    }
                    self.expect(&Token::RBrace)?;
                    
                    Ok(Item::System(SystemDef { name, functions, is_hot: true }))
                } else if self.check(&Token::Shader) {
                    self.advance();
                    Ok(Item::Shader(self.parse_shader(true)?))
                } else {
                    bail!("Expected 'system' or 'shader' after '@hot'");
                }
            }
            Token::Extern => {
                self.advance();
                Ok(Item::ExternFunction(self.parse_extern_function()?))
            }
            Token::Fn => {
                self.advance();
                Ok(Item::Function(self.parse_function()?))
            }
            _ => bail!("Unexpected token at item level: {:?}", self.peek()),
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
    
    fn parse_component(&mut self, is_soa: bool) -> Result<ComponentDef> {
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
        
        Ok(ComponentDef { name, fields, is_soa })
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
                bail!("Expected shader stage (vertex, fragment, compute, etc.)");
            }
        };
        
        // Parse shader path: "path/to/shader.glsl"
        let path = if let Token::StringLit(ref path) = *self.peek() {
            let path = path.clone();
            self.advance();
            path
        } else {
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
                bail!("Expected '}}' to close shader declaration");
            }
            self.advance();
        }
        
        Ok(crate::ast::ShaderDef { stage, path, is_hot })
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
            _ => bail!("Unexpected token in type: {:?}", self.peek()),
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
    
    fn parse_statement(&mut self) -> Result<Statement> {
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
                Ok(Statement::Let { name, ty, value })
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
                Ok(Statement::While { condition, body })
            }
            Token::For => {
                // Parse: for <iterator> in <collection> { ... }
                self.advance();
                let iterator = self.expect_ident()?;
                self.expect(&Token::In)?;
                let collection = self.parse_expression()?;
                let body = self.parse_block()?;
                Ok(Statement::For { iterator, collection, body })
            }
            Token::Loop => {
                self.advance();
                let body = self.parse_block()?;
                Ok(Statement::Loop { body })
            }
            Token::Return => {
                self.advance();
                let expr = if !self.check(&Token::Semicolon) {
                    Some(self.parse_expression()?)
                } else {
                    None
                };
                self.expect(&Token::Semicolon)?;
                Ok(Statement::Return(expr))
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
                    })
                } else {
                    self.expect(&Token::Semicolon)?;
                    Ok(Statement::Expression(expr))
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
            self.advance();
            let right = self.parse_and()?;
            expr = Expression::BinaryOp {
                op: BinaryOp::Or,
                left: Box::new(expr),
                right: Box::new(right),
            };
        }
        
        Ok(expr)
    }
    
    fn parse_and(&mut self) -> Result<Expression> {
        let mut expr = self.parse_equality()?;
        
        while self.check(&Token::AndAnd) {
            self.advance();
            let right = self.parse_equality()?;
            expr = Expression::BinaryOp {
                op: BinaryOp::And,
                left: Box::new(expr),
                right: Box::new(right),
            };
        }
        
        Ok(expr)
    }
    
    fn parse_equality(&mut self) -> Result<Expression> {
        let mut expr = self.parse_comparison()?;
        
        while self.check(&Token::EqEq) || self.check(&Token::Ne) {
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
            };
        }
        
        Ok(expr)
    }
    
    fn parse_comparison(&mut self) -> Result<Expression> {
        let mut expr = self.parse_term()?;
        
        while matches!(self.peek(), Token::Lt | Token::Le | Token::Gt | Token::Ge) {
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
            };
        }
        
        Ok(expr)
    }
    
    fn parse_term(&mut self) -> Result<Expression> {
        let mut expr = self.parse_factor()?;
        
        while self.check(&Token::Plus) || self.check(&Token::Minus) {
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
            };
        }
        
        Ok(expr)
    }
    
    fn parse_factor(&mut self) -> Result<Expression> {
        let mut expr = self.parse_unary()?;
        
        while self.check(&Token::Star) || self.check(&Token::Slash) || self.check(&Token::Percent) {
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
            };
        }
        
        Ok(expr)
    }
    
    fn parse_unary(&mut self) -> Result<Expression> {
        if self.check(&Token::Bang) {
            self.advance();
            let expr = self.parse_unary()?;
            return Ok(Expression::UnaryOp {
                op: UnaryOp::Not,
                expr: Box::new(expr),
            });
        }
        
        if self.check(&Token::Minus) {
            self.advance();
            let expr = self.parse_unary()?;
            return Ok(Expression::UnaryOp {
                op: UnaryOp::Neg,
                expr: Box::new(expr),
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
                
                if let Expression::Variable(name) = expr {
                    expr = Expression::Call { name, args };
                } else {
                    bail!("Expected function name");
                }
            } else if self.check(&Token::Dot) {
                self.advance();
                let member = self.expect_ident()?;
                expr = Expression::MemberAccess {
                    object: Box::new(expr),
                    member,
                };
            } else if self.check(&Token::LBracket) {
                self.advance();
                let index = self.parse_expression()?;
                self.expect(&Token::RBracket)?;
                expr = Expression::Index {
                    array: Box::new(expr),
                    index: Box::new(index),
                };
            } else {
                break;
            }
        }
        
        Ok(expr)
    }
    
    fn parse_primary(&mut self) -> Result<Expression> {
        let token = self.peek().clone();
        match token {
            Token::Int(n) => {
                self.advance();
                Ok(Expression::Literal(Literal::Int(n)))
            }
            Token::Float(n) => {
                self.advance();
                Ok(Expression::Literal(Literal::Float(n)))
            }
            Token::True => {
                self.advance();
                Ok(Expression::Literal(Literal::Bool(true)))
            }
            Token::False => {
                self.advance();
                Ok(Expression::Literal(Literal::Bool(false)))
            }
            Token::StringLit(s) => {
                self.advance();
                Ok(Expression::Literal(Literal::String(s)))
            }
            Token::Ident(name) => {
                self.advance();
                Ok(Expression::Variable(name))
            }
            Token::LParen => {
                self.advance();
                let expr = self.parse_expression()?;
                self.expect(&Token::RParen)?;
                Ok(expr)
            }
            _ => bail!("Unexpected token in expression: {:?}", self.peek()),
        }
    }
    
    fn expect_ident(&mut self) -> Result<String> {
        let token = self.peek().clone();
        match token {
            Token::Ident(name) => {
                self.advance();
                Ok(name)
            }
            _ => bail!("Expected identifier, got {:?}", self.peek()),
        }
    }
    
    fn expect(&mut self, token: &Token) -> Result<()> {
        if self.check(token) {
            self.advance();
            Ok(())
        } else {
            bail!("Expected {:?}, got {:?}", token, self.peek())
        }
    }
    
    fn check(&self, token: &Token) -> bool {
        !self.is_at_end() && std::mem::discriminant(self.peek()) == std::mem::discriminant(token)
    }
    
    fn peek(&self) -> &Token {
        &self.tokens[self.current].token
    }
    
    fn peek_location(&self) -> SourceLocation {
        if self.current < self.tokens.len() {
            self.tokens[self.current].location
        } else {
            SourceLocation::unknown()
        }
    }
    
    fn advance(&mut self) {
        if !self.is_at_end() {
            self.current_location = self.tokens[self.current].location;
            self.current += 1;
        }
    }
    
    fn current_location(&self) -> SourceLocation {
        self.current_location
    }
    
    fn is_at_end(&self) -> bool {
        self.current >= self.tokens.len()
    }
}

