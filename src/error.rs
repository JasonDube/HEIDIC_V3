// Error reporting module for HEIDIC compiler
// Provides enhanced error messages with source location, context, and suggestions

use std::fs;

#[derive(Debug, Clone, Copy)]
pub struct SourceLocation {
    pub line: usize,      // 1-based line number
    pub column: usize,    // 1-based column number (character position in line)
}

impl SourceLocation {
    pub fn new(line: usize, column: usize) -> Self {
        Self { line, column }
    }
    
    pub fn unknown() -> Self {
        Self { line: 0, column: 0 }
    }
    
    pub fn is_unknown(&self) -> bool {
        self.line == 0 && self.column == 0
    }
}

pub struct ErrorReporter {
    file_path: String,
    source_lines: Vec<String>,
}

impl ErrorReporter {
    pub fn new(file_path: &str) -> anyhow::Result<Self> {
        let source = fs::read_to_string(file_path)?;
        let source_lines: Vec<String> = source.lines().map(|s| s.to_string()).collect();
        
        Ok(Self {
            file_path: file_path.to_string(),
            source_lines,
        })
    }
    
    pub fn report_error(&self, location: SourceLocation, message: &str, suggestion: Option<&str>) {
        if location.is_unknown() {
            eprintln!("Error: {}", message);
            if let Some(sug) = suggestion {
                eprintln!("Suggestion: {}", sug);
            }
            return;
        }
        
        // Print error header
        eprintln!("Error at {}:{}:{}:", 
                 self.file_path, location.line, location.column);
        
        // Print source line with context
        if location.line > 0 && location.line <= self.source_lines.len() {
            let line_content = &self.source_lines[location.line - 1];
            eprintln!("    {}", line_content);
            
            // Print caret pointing to error location
            let spaces = if location.column > 0 {
                location.column - 1
            } else {
                0
            };
            let caret = " ".repeat(spaces) + &"^".repeat(
                if location.column > 0 && location.column <= line_content.len() {
                    // Point to the character or word
                    let remaining = &line_content[spaces..];
                    remaining.chars().next().map(|c| c.len_utf8()).unwrap_or(1)
                } else {
                    1
                }
            );
            eprintln!("    {}", caret);
        }
        
        // Print error message
        eprintln!("{}", message);
        
        // Print suggestion if provided
        if let Some(sug) = suggestion {
            eprintln!("Suggestion: {}", sug);
        }
        
        eprintln!(); // Blank line for readability
    }
    
    pub fn report_error_simple(&self, location: SourceLocation, message: &str) {
        self.report_error(location, message, None);
    }
}

