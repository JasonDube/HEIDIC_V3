use anyhow::{Result, bail};
use crate::lexer::Token;

#[derive(Debug, Clone)]
pub enum HeirocExpr {
    Ident(String),
    String(String),
    Number(f64),
    Integer(i64),
    Assignment {
        name: String,
        value: Box<HeirocExpr>,
    },
}

#[derive(Debug, Clone)]
pub struct PanelDef {
    pub name: String,
    pub properties: Vec<(String, HeirocExpr)>,
}

#[derive(Debug, Clone)]
pub struct MainLoopParams {
    pub video_resolution: Option<i64>,
    pub video_mode: Option<i64>,
    pub fps_max: Option<i64>,
    pub random_seed: Option<i64>,
    pub load_level: Option<String>,
}

#[derive(Debug, Clone)]
pub enum HeirocStmt {
    Panel(PanelDef),
    MainLoop(MainLoopParams),
    // Future: If, While, etc.
}

#[derive(Debug, Clone)]
pub struct HeirocProgram {
    pub statements: Vec<HeirocStmt>,
}

pub struct Parser {
    tokens: Vec<Token>,
    position: usize,
}

impl Parser {
    pub fn new(tokens: Vec<Token>) -> Self {
        Self {
            tokens,
            position: 0,
        }
    }
    
    pub fn parse(&mut self) -> Result<HeirocProgram> {
        let mut statements = Vec::new();
        
        while !self.is_at_end() {
            if self.check(&Token::EOF) {
                break;
            }
            
            // Skip comments
            if matches!(self.peek(), Token::Comment(_)) {
                self.advance();
                continue;
            }
            
            // Parse PANEL* declarations
            if self.check(&Token::Panel) {
                statements.push(HeirocStmt::Panel(self.parse_panel()?));
            }
            // Parse main_loop()
            else if self.check(&Token::MainLoop) {
                statements.push(HeirocStmt::MainLoop(self.parse_main_loop()?));
            }
            else {
                self.advance(); // Skip unknown tokens for now
            }
        }
        
        Ok(HeirocProgram { statements })
    }
    
    fn parse_panel(&mut self) -> Result<PanelDef> {
        // PANEL* name = { ... }
        self.consume(&Token::Panel)?;
        self.consume(&Token::Asterisk)?;
        
        let name = match self.advance() {
            Token::Ident(name) => name,
            _ => bail!("Expected panel name"),
        };
        
        self.consume(&Token::Assign)?;
        self.consume(&Token::LBrace)?;
        
        let mut properties = Vec::new();
        while !self.check(&Token::RBrace) && !self.is_at_end() {
            let prop_name = match self.advance() {
                Token::Ident(name) => name,
                _ => bail!("Expected property name"),
            };
            
            self.consume(&Token::Assign)?;
            let value = self.parse_expr()?;
            self.consume(&Token::Semicolon)?;
            
            properties.push((prop_name, value));
        }
        
        self.consume(&Token::RBrace)?;
        
        Ok(PanelDef { name, properties })
    }
    
    fn parse_main_loop(&mut self) -> Result<MainLoopParams> {
        // main_loop( ... )
        self.consume(&Token::MainLoop)?;
        self.consume(&Token::LParen)?;
        
        let mut params = MainLoopParams {
            video_resolution: None,
            video_mode: None,
            fps_max: None,
            random_seed: None,
            load_level: None,
        };
        
        while !self.check(&Token::RParen) && !self.is_at_end() {
            // Skip comments
            while matches!(self.peek(), Token::Comment(_)) {
                self.advance();
            }
            
            // Check if we've hit the closing paren after skipping comments
            if self.check(&Token::RParen) {
                break;
            }
            
            let param_name = match self.advance() {
                Token::Ident(name) => name,
                other => bail!("Expected parameter name, got {:?}", other),
            };
            
            self.consume(&Token::Assign)?;
            
            match param_name.as_str() {
                "video_resolution" => {
                    params.video_resolution = Some(self.parse_integer()?);
                }
                "video_mode" => {
                    params.video_mode = Some(self.parse_integer()?);
                }
                "fps_max" => {
                    params.fps_max = Some(self.parse_integer()?);
                }
                "random_seed" => {
                    params.random_seed = Some(self.parse_integer()?);
                }
                "load_level" => {
                    params.load_level = Some(self.parse_string()?);
                }
                _ => {
                    // Unknown parameter, skip
                    self.parse_expr()?;
                }
            }
            
            if self.check(&Token::Semicolon) {
                self.advance();
            }
        }
        
        self.consume(&Token::RParen)?;
        
        Ok(params)
    }
    
    fn parse_expr(&mut self) -> Result<HeirocExpr> {
        match self.advance() {
            Token::Ident(name) => Ok(HeirocExpr::Ident(name)),
            Token::String(s) => Ok(HeirocExpr::String(s)),
            Token::Number(n) => Ok(HeirocExpr::Number(n)),
            Token::Integer(i) => Ok(HeirocExpr::Integer(i)),
            _ => bail!("Unexpected token in expression"),
        }
    }
    
    fn parse_integer(&mut self) -> Result<i64> {
        match self.advance() {
            Token::Integer(i) => Ok(i),
            _ => bail!("Expected integer"),
        }
    }
    
    fn parse_string(&mut self) -> Result<String> {
        match self.advance() {
            Token::String(s) => Ok(s),
            _ => bail!("Expected string"),
        }
    }
    
    fn consume(&mut self, expected: &Token) -> Result<()> {
        if self.check(expected) {
            self.advance();
            Ok(())
        } else {
            bail!("Expected {:?}, got {:?}", expected, self.peek())
        }
    }
    
    fn check(&self, token: &Token) -> bool {
        if self.is_at_end() {
            false
        } else {
            std::mem::discriminant(&self.tokens[self.position]) == std::mem::discriminant(token)
        }
    }
    
    fn advance(&mut self) -> Token {
        if self.is_at_end() {
            Token::EOF
        } else {
            let token = self.tokens[self.position].clone();
            self.position += 1;
            token
        }
    }
    
    fn peek(&self) -> &Token {
        if self.is_at_end() {
            &Token::EOF
        } else {
            &self.tokens[self.position]
        }
    }
    
    fn is_at_end(&self) -> bool {
        self.position >= self.tokens.len() || matches!(self.tokens[self.position], Token::EOF)
    }
}

