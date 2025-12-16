use anyhow::{Result, bail};

#[derive(Debug, Clone, PartialEq)]
pub enum Token {
    // Identifiers and literals
    Ident(String),
    String(String),
    Number(f64),
    Integer(i64),
    
    // Keywords
    MainLoop,
    Panel,
    If,
    While,
    Wait,
    
    // Operators
    Assign,      // =
    Semicolon,   // ;
    Comma,       // ,
    Colon,       // :
    Asterisk,    // *
    
    // Brackets
    LParen,      // (
    RParen,      // )
    LBrace,      // {
    RBrace,      // }
    LBracket,    // [
    RBracket,    // ]
    
    // Comments
    Comment(String),
    
    // Special
    EOF,
}

pub struct Lexer {
    input: Vec<char>,
    position: usize,
}

impl Lexer {
    pub fn new(input: &str) -> Self {
        Self {
            input: input.chars().collect(),
            position: 0,
        }
    }
    
    pub fn tokenize(&mut self) -> Result<Vec<Token>> {
        let mut tokens = Vec::new();
        
        while !self.is_at_end() {
            self.skip_whitespace();
            if self.is_at_end() {
                break;
            }
            
            let token = self.next_token()?;
            tokens.push(token);
        }
        
        tokens.push(Token::EOF);
        Ok(tokens)
    }
    
    fn next_token(&mut self) -> Result<Token> {
        let ch = self.advance();
        
        match ch {
            '(' => Ok(Token::LParen),
            ')' => Ok(Token::RParen),
            '{' => Ok(Token::LBrace),
            '}' => Ok(Token::RBrace),
            '[' => Ok(Token::LBracket),
            ']' => Ok(Token::RBracket),
            '=' => Ok(Token::Assign),
            ';' => Ok(Token::Semicolon),
            ',' => Ok(Token::Comma),
            ':' => Ok(Token::Colon),
            '*' => Ok(Token::Asterisk),
            '/' => {
                if self.peek() == '/' {
                    // Line comment
                    let mut comment = String::new();
                    while !self.is_at_end() && self.peek() != '\n' {
                        comment.push(self.advance());
                    }
                    Ok(Token::Comment(comment))
                } else {
                    bail!("Unexpected character: /")
                }
            }
            '\'' | '"' => {
                let quote = ch;
                let mut string = String::new();
                while !self.is_at_end() && self.peek() != quote {
                    string.push(self.advance());
                }
                if self.is_at_end() {
                    bail!("Unterminated string literal");
                }
                self.advance(); // consume closing quote
                Ok(Token::String(string))
            }
            _ if ch.is_alphabetic() || ch == '_' => {
                let mut ident = String::from(ch);
                while !self.is_at_end() && (self.peek().is_alphanumeric() || self.peek() == '_') {
                    ident.push(self.advance());
                }
                Ok(self.keyword_or_ident(&ident))
            }
            _ if ch.is_ascii_digit() => {
                let mut num_str = String::from(ch);
                while !self.is_at_end() && self.peek().is_ascii_digit() {
                    num_str.push(self.advance());
                }
                if self.peek() == '.' {
                    num_str.push(self.advance());
                    while !self.is_at_end() && self.peek().is_ascii_digit() {
                        num_str.push(self.advance());
                    }
                    Ok(Token::Number(num_str.parse()?))
                } else {
                    Ok(Token::Integer(num_str.parse()?))
                }
            }
            _ => bail!("Unexpected character: {}", ch),
        }
    }
    
    fn keyword_or_ident(&self, ident: &str) -> Token {
        match ident {
            "main_loop" => Token::MainLoop,
            "PANEL" => Token::Panel,
            "if" => Token::If,
            "while" => Token::While,
            "wait" => Token::Wait,
            _ => Token::Ident(ident.to_string()),
        }
    }
    
    fn advance(&mut self) -> char {
        if self.is_at_end() {
            '\0'
        } else {
            let ch = self.input[self.position];
            self.position += 1;
            ch
        }
    }
    
    fn peek(&self) -> char {
        if self.is_at_end() {
            '\0'
        } else {
            self.input[self.position]
        }
    }
    
    fn skip_whitespace(&mut self) {
        while !self.is_at_end() && self.peek().is_whitespace() {
            self.advance();
        }
    }
    
    fn is_at_end(&self) -> bool {
        self.position >= self.input.len()
    }
}

