#[derive(Debug, Clone)]
pub enum Type {
    I32,
    I64,
    F32,
    F64,
    Bool,
    String,
    Array(Box<Type>),
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
pub struct FunctionDef {
    pub name: String,
    pub params: Vec<Param>,
    pub return_type: Type,
    pub body: Vec<Statement>,
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
pub struct Param {
    pub name: String,
    pub ty: Type,
}

#[derive(Debug, Clone)]
pub enum Statement {
    Let { name: String, ty: Option<Type>, value: Expression },
    Assign { target: Expression, value: Expression },
    If { condition: Expression, then_block: Vec<Statement>, else_block: Option<Vec<Statement>> },
    While { condition: Expression, body: Vec<Statement> },
    For { iterator: String, collection: Expression, body: Vec<Statement> },
    Loop { body: Vec<Statement> },
    Return(Option<Expression>),
    Expression(Expression),
    #[allow(dead_code)] // Block statements not yet fully implemented
    Block(Vec<Statement>),
}

#[derive(Debug, Clone)]
pub enum Expression {
    Literal(Literal),
    Variable(String),
    BinaryOp { op: BinaryOp, left: Box<Expression>, right: Box<Expression> },
    UnaryOp { op: UnaryOp, expr: Box<Expression> },
    Call { name: String, args: Vec<Expression> },
    MemberAccess { object: Box<Expression>, member: String },
    Index { array: Box<Expression>, index: Box<Expression> },
    #[allow(dead_code)] // Struct literals not yet fully implemented
    StructLiteral { name: String, fields: Vec<(String, Expression)> },
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

