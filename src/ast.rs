use crate::error::SourceLocation;

#[derive(Debug, Clone)]
pub enum Type {
    I32,
    I64,
    F32,
    F64,
    Bool,
    String,
    Array(Box<Type>),
    Optional(Box<Type>),  // ?Type - optional type
    Struct(String),
    #[allow(dead_code)] // Component system not yet fully implemented
    Component(String),
    Query(Vec<Type>), // query<Component1, Component2, ...>
    Void,
    // Vulkan types
    VkInstance,
    VkDevice,
    VkResult,
    VkPhysicalDevice,
    VkQueue,
    VkCommandPool,
    VkCommandBuffer,
    VkSwapchainKHR,
    VkSurfaceKHR,
    VkRenderPass,
    VkPipeline,
    VkFramebuffer,
    VkBuffer,
    VkImage,
    VkImageView,
    VkSemaphore,
    VkFence,
    // GLFW types
    GLFWwindow,
    GLFWbool,
    // Math types (mapped to GLM)
    Vec2,
    Vec3,
    Vec4,
    Mat4,
    // Error type (poison type for error recovery)
    Error,  // Represents a type error - propagates through operations
}

#[derive(Debug, Clone)]
pub struct Program {
    pub items: Vec<Item>,
}

#[derive(Debug, Clone)]
pub enum Item {
    Struct(StructDef),
    Component(ComponentDef),
    System(SystemDef),
    Shader(ShaderDef),
    Function(FunctionDef),
    ExternFunction(ExternFunctionDef),
    Resource(ResourceDef),
    Pipeline(PipelineDef),
}

#[derive(Debug, Clone)]
pub struct StructDef {
    pub name: String,
    pub fields: Vec<Field>,
}

#[derive(Debug, Clone)]
pub struct ComponentDef {
    pub name: String,
    pub fields: Vec<Field>,
    pub is_soa: bool,  // true if component_soa, false if regular component
    pub is_hot: bool,  // true if marked with @hot
    pub is_cuda: bool,  // true if marked with @[cuda]
}

#[derive(Debug, Clone)]
pub struct SystemDef {
    pub name: String,
    pub functions: Vec<FunctionDef>,
    pub is_hot: bool,  // true if marked with @hot
}

#[derive(Debug, Clone)]
pub struct ShaderDef {
    pub stage: ShaderStage,
    pub path: String,  // Path to shader source file
    pub is_hot: bool,  // true if marked with @hot
}

#[derive(Debug, Clone, PartialEq)]
pub enum ShaderStage {
    Vertex,
    Fragment,
    Compute,
    Geometry,
    TessellationControl,
    TessellationEvaluation,
}

#[derive(Debug, Clone)]
pub struct Field {
    pub name: String,
    pub ty: Type,
}

#[derive(Debug, Clone)]
pub struct Param {
    pub name: String,
    pub ty: Type,
}

#[derive(Debug, Clone)]
pub struct FunctionDef {
    pub name: String,
    pub params: Vec<Param>,
    pub return_type: Type,
    pub body: Vec<Statement>,
    pub cuda_kernel: Option<String>,  // Some(kernel_name) if marked with @[launch(kernel = name)]
}

#[derive(Debug, Clone)]
pub struct ExternFunctionDef {
    pub name: String,
    pub params: Vec<Param>,
    pub return_type: Type,
    pub library: Option<String>, // Library name to link against
}

#[derive(Debug, Clone)]
pub struct ResourceDef {
    pub name: String,
    pub resource_type: String, // "Texture", "Mesh", etc.
    pub path: String,          // File path (string literal)
    pub is_hot: bool,          // true if marked with @hot
}

#[derive(Debug, Clone)]
pub struct PipelineDef {
    pub name: String,
    pub shaders: Vec<PipelineShader>,  // Shader stage and path
    pub layout: Option<PipelineLayout>, // Optional descriptor set layout
}

#[derive(Debug, Clone)]
pub struct PipelineShader {
    pub stage: ShaderStage,
    pub path: String,  // Path to shader file
}

#[derive(Debug, Clone)]
pub struct PipelineLayout {
    pub bindings: Vec<LayoutBinding>,
}

#[derive(Debug, Clone)]
pub struct LayoutBinding {
    pub binding: u32,  // Binding index
    pub binding_type: BindingType,
    pub name: String,  // Resource name (for reference)
}

#[derive(Debug, Clone, PartialEq)]
pub enum BindingType {
    Uniform(String),      // uniform TypeName
    Storage(String),      // storage TypeName[]
    Sampler2D,           // sampler2D
}

#[derive(Debug, Clone)]
pub enum Statement {
    Let { name: String, ty: Option<Type>, value: Expression, location: SourceLocation },
    Assign { target: Expression, value: Expression, location: SourceLocation },
    If { condition: Expression, then_block: Vec<Statement>, else_block: Option<Vec<Statement>>, location: SourceLocation },
    While { condition: Expression, body: Vec<Statement>, location: SourceLocation },
    For { iterator: String, collection: Expression, body: Vec<Statement>, location: SourceLocation },
    Loop { body: Vec<Statement>, location: SourceLocation },
    Return(Option<Expression>, SourceLocation),
    Break(SourceLocation),
    Continue(SourceLocation),
    Defer(Box<Expression>, SourceLocation),  // defer expr; - executes at scope exit
    Expression(Expression, SourceLocation),
    #[allow(dead_code)] // Block statements not yet fully implemented
    Block(Vec<Statement>, SourceLocation),
}

#[derive(Debug, Clone)]
pub enum Expression {
    Literal(Literal, SourceLocation),
    Variable(String, SourceLocation),
    BinaryOp { op: BinaryOp, left: Box<Expression>, right: Box<Expression>, location: SourceLocation },
    UnaryOp { op: UnaryOp, expr: Box<Expression>, location: SourceLocation },
    Call { name: String, args: Vec<Expression>, location: SourceLocation },
    MemberAccess { object: Box<Expression>, member: String, location: SourceLocation },
    Index { array: Box<Expression>, index: Box<Expression>, location: SourceLocation },
    ArrayLiteral { elements: Vec<Expression>, location: SourceLocation },
    StringInterpolation { parts: Vec<StringInterpolationPart>, location: SourceLocation },
    Match { expr: Box<Expression>, arms: Vec<MatchArm>, location: SourceLocation },
    #[allow(dead_code)] // Struct literals not yet fully implemented
    StructLiteral { name: String, fields: Vec<(String, Expression)>, location: SourceLocation },
}

#[derive(Debug, Clone)]
pub struct MatchArm {
    pub pattern: Pattern,
    pub body: Vec<Statement>,
    pub location: SourceLocation,
}

#[derive(Debug, Clone)]
pub enum Pattern {
    Literal(Literal, SourceLocation),
    Variable(String, SourceLocation),
    Wildcard(SourceLocation),  // _ pattern
    Ident(String, SourceLocation),  // For enum variants or constants (e.g., VK_SUCCESS)
}

#[derive(Debug, Clone)]
pub enum StringInterpolationPart {
    Literal(String),
    Variable(String),
}

#[derive(Debug, Clone)]
pub enum Literal {
    Int(i64),
    Float(f64),
    Bool(bool),
    String(String),
}

#[derive(Debug, Clone)]
pub enum BinaryOp {
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,
    And,
    Or,
}

#[derive(Debug, Clone)]
pub enum UnaryOp {
    Neg,
    Not,
}

// Helper methods to extract source locations from AST nodes
impl Statement {
    pub fn location(&self) -> SourceLocation {
        match self {
            Statement::Let { location, .. } => *location,
            Statement::Assign { location, .. } => *location,
            Statement::If { location, .. } => *location,
            Statement::While { location, .. } => *location,
            Statement::For { location, .. } => *location,
            Statement::Loop { location, .. } => *location,
            Statement::Return(_, location) => *location,
            Statement::Break(location) => *location,
            Statement::Continue(location) => *location,
            Statement::Defer(_, location) => *location,
            Statement::Expression(_, location) => *location,
            Statement::Block(_, location) => *location,
        }
    }
}

impl Expression {
    pub fn location(&self) -> SourceLocation {
        match self {
            Expression::Literal(_, location) => *location,
            Expression::Variable(_, location) => *location,
            Expression::BinaryOp { location, .. } => *location,
            Expression::UnaryOp { location, .. } => *location,
            Expression::Call { location, .. } => *location,
            Expression::MemberAccess { location, .. } => *location,
            Expression::Index { location, .. } => *location,
            Expression::ArrayLiteral { location, .. } => *location,
            Expression::StringInterpolation { location, .. } => *location,
            Expression::Match { location, .. } => *location,
            Expression::StructLiteral { location, .. } => *location,
        }
    }
}

