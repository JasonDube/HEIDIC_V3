use std::fs;
use anyhow::{Context, Result};

mod lexer;
mod parser;
mod codegen;

use lexer::Lexer;
use parser::Parser;
use codegen::CodeGenerator;

fn main() -> Result<()> {
    let args: Vec<String> = std::env::args().collect();
    
    if args.len() < 2 {
        eprintln!("Usage: heiroc_transpiler <command> [args...]");
        eprintln!("Commands:");
        eprintln!("  transpile <input.heiroc> <output.hd>  - Transpile HEIROC to HEIDIC");
        return Ok(());
    }
    
    let command = &args[1];
    
    match command.as_str() {
        "transpile" => {
            if args.len() < 4 {
                anyhow::bail!("Usage: heiroc_transpiler transpile <input.heiroc> <output.hd>");
            }
            let input_path = &args[2];
            let output_path = &args[3];
            transpile_file(input_path, output_path)?;
        }
        _ => {
            anyhow::bail!("Unknown command: {}. Use 'transpile'", command);
        }
    }
    
    Ok(())
}

fn transpile_file(input_path: &str, output_path: &str) -> Result<()> {
    let input = fs::read_to_string(input_path)
        .with_context(|| format!("Failed to read input file: {}", input_path))?;
    
    // Lex HEIROC source
    let mut lexer = Lexer::new(&input);
    let tokens = lexer.tokenize()?;
    
    // Parse HEIROC AST
    let mut parser = Parser::new(tokens);
    let program = parser.parse()?;
    
    // Find project directory (parent of input file)
    let project_dir = std::path::Path::new(input_path)
        .parent()
        .unwrap_or_else(|| std::path::Path::new("."));
    
    // Generate HEIDIC code
    let mut codegen = CodeGenerator::new();
    let heidic_code = codegen.generate(&program, project_dir)?;
    
    // Write output
    fs::write(output_path, heidic_code)
        .with_context(|| format!("Failed to write output file: {}", output_path))?;
    
    println!("Transpiled {} -> {}", input_path, output_path);
    Ok(())
}
