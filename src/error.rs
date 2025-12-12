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

#[derive(Clone)]
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
        self.report_error_with_secondary(location, message, suggestion, None, None);
    }
    
    pub fn report_error_with_secondary(
        &self, 
        location: SourceLocation, 
        message: &str, 
        suggestion: Option<&str>,
        secondary_location: Option<SourceLocation>,
        secondary_label: Option<&str>,
    ) {
        if location.is_unknown() {
            eprintln!("❌ Error: {}", message);
            if let Some(sug) = suggestion {
                eprintln!("💡 Suggestion: {}", sug);
            }
            eprintln!();
            return;
        }
        
        // Print error header with emoji for better visibility
        eprintln!("❌ Error at {}:{}:{}:", 
                 self.file_path, location.line, location.column);
        
        // Print source line with context (show previous and next lines if available)
        if location.line > 0 && location.line <= self.source_lines.len() {
            // Show previous line for context
            if location.line > 1 {
                let prev_line = &self.source_lines[location.line - 2];
                eprintln!("  {} | {}", location.line - 1, prev_line);
            }
            
            // Show current line with error
            let line_content = &self.source_lines[location.line - 1];
            eprintln!("  {} | {}", location.line, line_content);
            
            // Print caret pointing to error location
            let spaces = if location.column > 0 {
                location.column - 1
            } else {
                0
            };
            
            // Calculate caret width (point to the token/word)
            let caret_width = if location.column > 0 && location.column <= line_content.len() {
                // Try to find the end of the current token
                let remaining = &line_content[spaces..];
                let mut width = 0;
                for ch in remaining.chars() {
                    if ch.is_alphanumeric() || ch == '_' {
                        width += ch.len_utf8();
                    } else {
                        width = width.max(1);
                        break;
                    }
                }
                width.max(1)
            } else {
                1
            };
            
            let line_num_spaces = location.line.to_string().len() + 3; // "  X | "
            let caret = " ".repeat(line_num_spaces + spaces) + &"^".repeat(caret_width);
            eprintln!("{}", caret);
            
            // Show next line for context
            if location.line < self.source_lines.len() {
                let next_line = &self.source_lines[location.line];
                eprintln!("  {} | {}", location.line + 1, next_line);
            }
        }
        
        // Print secondary location if provided
        if let Some(sec_loc) = secondary_location {
            if !sec_loc.is_unknown() && sec_loc.line > 0 && sec_loc.line <= self.source_lines.len() {
                let label = secondary_label.unwrap_or("Note: defined here");
                eprintln!("\n📌 {} at {}:{}:{}:", 
                         label, self.file_path, sec_loc.line, sec_loc.column);
                
                // Show context around secondary location
                if sec_loc.line > 1 {
                    let prev_line = &self.source_lines[sec_loc.line - 2];
                    eprintln!("  {} | {}", sec_loc.line - 1, prev_line);
                }
                
                let line_content = &self.source_lines[sec_loc.line - 1];
                eprintln!("  {} | {}", sec_loc.line, line_content);
                
                // Print caret for secondary location
                let spaces = if sec_loc.column > 0 {
                    sec_loc.column - 1
                } else {
                    0
                };
                
                let caret_width = if sec_loc.column > 0 && sec_loc.column <= line_content.len() {
                    let remaining = &line_content[spaces..];
                    let mut width = 0;
                    for ch in remaining.chars() {
                        if ch.is_alphanumeric() || ch == '_' {
                            width += ch.len_utf8();
                        } else {
                            width = width.max(1);
                            break;
                        }
                    }
                    width.max(1)
                } else {
                    1
                };
                
                let line_num_spaces = sec_loc.line.to_string().len() + 3;
                let caret = " ".repeat(line_num_spaces + spaces) + &"^".repeat(caret_width);
                eprintln!("{}", caret);
                
                if sec_loc.line < self.source_lines.len() {
                    let next_line = &self.source_lines[sec_loc.line];
                    eprintln!("  {} | {}", sec_loc.line + 1, next_line);
                }
            }
        }
        
        // Print error message
        eprintln!("\n{}", message);
        
        // Print suggestion if provided
        if let Some(sug) = suggestion {
            eprintln!("💡 Suggestion: {}", sug);
        }
        
        eprintln!(); // Blank line for readability
    }
}

