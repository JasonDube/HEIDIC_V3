use logos::Logos;
use anyhow::{Result, bail};

#[derive(Logos, Debug, PartialEq, Clone)]
#[logos(skip r"[ \t\n\r]+")]
#[logos(skip r"//[^\n]*")]
pub enum Token {
    // Keywords
    #[token("fn")]
    Fn,
    #[token("let")]
    Let,
    #[token("if")]
    If,
    #[token("else")]
    Else,
    #[token("while")]
    While,
    #[token("loop")]
    Loop,
    #[token("return")]
    Return,
    #[token("defer")]
    Defer,
    #[token("struct")]
    Struct,
    #[token("component")]
    Component,
    #[token("component_soa")]
    ComponentSOA,
    #[token("system")]
    System,
    #[token("shader")]
    Shader,
    #[token("vertex")]
    Vertex,
    #[token("fragment")]
    Fragment,
    #[token("compute")]
    Compute,
    #[token("geometry")]
    Geometry,
    #[token("tessellation_control")]
    TessellationControl,
    #[token("tessellation_evaluation")]
    TessellationEvaluation,
    #[token("for")]
    For,
    #[token("in")]
    In,
    #[token("match")]
    Match,
    #[token("query")]
    Query,
    #[token("extern")]
    Extern,
    #[token("resource")]
    Resource,
    #[token("pipeline")]
    Pipeline,
    #[token("uniform")]
    Uniform,
    #[token("storage")]
    Storage,
    #[token("sampler2D")]
    Sampler2D,
    #[token("binding")]
    Binding,
    #[token("layout")]
    Layout,
    
    // Attributes
    #[token("@hot")]
    Hot,
    #[token("@")]
    At,
    
    // Types
    #[token("i32")]
    I32,
    #[token("i64")]
    I64,
    #[token("f32")]
    F32,
    #[token("f64")]
    F64,
    #[token("bool")]
    Bool,
    #[token("string")]
    String,
    #[token("void")]
    Void,
    
    // Vulkan types
    #[token("VkInstance")]
    VkInstance,
    #[token("VkDevice")]
    VkDevice,
    #[token("VkResult")]
    VkResult,
    #[token("VkPhysicalDevice")]
    VkPhysicalDevice,
    #[token("VkQueue")]
    VkQueue,
    #[token("VkCommandPool")]
    VkCommandPool,
    #[token("VkCommandBuffer")]
    VkCommandBuffer,
    #[token("VkSwapchainKHR")]
    VkSwapchainKHR,
    #[token("VkSurfaceKHR")]
    VkSurfaceKHR,
    #[token("VkRenderPass")]
    VkRenderPass,
    #[token("VkPipeline")]
    VkPipeline,
    #[token("VkFramebuffer")]
    VkFramebuffer,
    #[token("VkBuffer")]
    VkBuffer,
    #[token("VkImage")]
    VkImage,
    #[token("VkImageView")]
    VkImageView,
    #[token("VkSemaphore")]
    VkSemaphore,
    #[token("VkFence")]
    VkFence,
    
    // GLFW types
    #[token("GLFWwindow")]
    GLFWwindow,
    #[token("GLFWbool")]
    GLFWbool,
    
    // Math types (mapped to GLM)
    #[token("Vec2")]
    Vec2,
    #[token("Vec3")]
    Vec3,
    #[token("Vec4")]
    Vec4,
    #[token("Mat4")]
    Mat4,
    
    // Literals
    #[regex(r"0[xX][0-9A-Fa-f]+", |lex| {
        let slice = lex.slice();
        i64::from_str_radix(&slice[2..], 16).ok()
    })]
    #[regex(r"-?\d+", |lex| lex.slice().parse().ok())]
    Int(i64),
    #[regex(r"-?\d+\.\d+", |lex| lex.slice().parse().ok())]
    Float(f64),
    #[token("true")]
    True,
    #[token("false")]
    False,
    #[token("null")]
    Null,
    #[regex(r#""[^"]*""#, |lex| lex.slice()[1..lex.slice().len()-1].to_string())]
    StringLit(String),
    
    // Identifiers
    #[regex(r"[a-zA-Z_][a-zA-Z0-9_]*", |lex| lex.slice().to_string())]
    Ident(String),
    
    // Operators
    #[token("+")]
    Plus,
    #[token("-")]
    Minus,
    #[token("*")]
    Star,
    #[token("/")]
    Slash,
    #[token("%")]
    Percent,
    #[token("==")]
    EqEq,
    #[token("!=")]
    Ne,
    #[token("<")]
    Lt,
    #[token("<=")]
    Le,
    #[token(">")]
    Gt,
    #[token(">=")]
    Ge,
    #[token("&&")]
    AndAnd,
    #[token("||")]
    OrOr,
    #[token("!")]
    Bang,
    #[token("=")]
    Eq,
    #[token("?")]
    Question,
    
    // Delimiters
    #[token("(")]
    LParen,
    #[token(")")]
    RParen,
    #[token("{")]
    LBrace,
    #[token("}")]
    RBrace,
    #[token("[")]
    LBracket,
    #[token("]")]
    RBracket,
    #[token(",")]
    Comma,
    #[token(":")]
    Colon,
    #[token(";")]
    Semicolon,
    #[token(".")]
    Dot,
}

pub struct Lexer {
    source: String,
}

#[derive(Debug, Clone)]
pub struct TokenWithLocation {
    pub token: Token,
    pub location: crate::error::SourceLocation,
}

impl Lexer {
    pub fn new(source: &str) -> Self {
        Self {
            source: source.to_string(),
        }
    }
    
    fn byte_to_line_column(&self, byte_pos: usize) -> (usize, usize) {
        let mut line = 1;
        let mut column = 1;
        let mut current_byte = 0;
        
        for (i, ch) in self.source.char_indices() {
            if current_byte >= byte_pos {
                break;
            }
            
            if ch == '\n' {
                line += 1;
                column = 1;
            } else {
                column += 1;
            }
            
            current_byte = i + ch.len_utf8();
        }
        
        (line, column)
    }
    
    pub fn tokenize(&mut self) -> Result<Vec<TokenWithLocation>> {
        let mut lexer = Token::lexer(&self.source);
        let mut tokens = Vec::new();
        
        while let Some(token_result) = lexer.next() {
            match token_result {
                Ok(token) => {
                    let span = lexer.span();
                    let (line, column) = self.byte_to_line_column(span.start);
                    tokens.push(TokenWithLocation {
                        token,
                        location: crate::error::SourceLocation::new(line, column),
                    });
                }
                Err(_) => {
                    let span = lexer.span();
                    let (line, column) = self.byte_to_line_column(span.start);
                    bail!("Lexical error at {}:{}", line, column);
                }
            }
        }
        
        Ok(tokens)
    }
}

