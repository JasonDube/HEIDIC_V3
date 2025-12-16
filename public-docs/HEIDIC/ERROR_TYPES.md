# HEIDIC Compiler Error Types

This document lists all error types that the HEIDIC compiler can detect and report with improved error messages.

## Error Categories

### 1. Undefined Variable Errors
**Error Message:** `Undefined variable: '<name>'`  
**Location:** Line and column where the variable is used  
**Suggestion:** `Did you mean to declare it first? Use: let <name>: Type = value;`

**Example:**
```heidic
fn test(): void {
    let result: i32 = undefined_var + 5;  // Error: undefined variable
}
```

---

### 2. Type Mismatch in Assignment
**Error Message:** `Type mismatch in assignment: cannot assign '<type1>' to '<type2>'`  
**Location:** Line and column of the assignment  
**Suggestion:** `Ensure types match: <type1> should be <type2>`

**Example:**
```heidic
fn test(): void {
    let x: i32 = 10;
    x = "hello";  // Error: cannot assign string to i32
}
```

---

### 3. Type Mismatch in Let Declaration
**Error Message:** `Type mismatch: cannot assign '<type1>' to '<type2>'`  
**Location:** Line and column of the let statement  
**Suggestion:** `Use a <type2> variable or convert: <name> = <suggested_value>`

**Example:**
```heidic
fn test(): void {
    let num: f32 = "not a number";  // Error: type mismatch
}
```

---

### 4. Undefined Function
**Error Message:** `Undefined function: '<name>'`  
**Location:** Line and column of the function call  
**Suggestion:** `Did you mean to declare it? Use: fn <name>() { ... }`

**Example:**
```heidic
fn test(): void {
    let result: i32 = unknown_function(10, 20);  // Error: undefined function
}
```

---

### 5. Wrong Argument Count
**Error Message:** `Argument count mismatch for function '<name>': expected <N> arguments, got <M>`  
**Location:** Line and column of the function call  
**Suggestion:** `Call with <N> arguments: <name>(...)`

**Example:**
```heidic
fn helper(a: i32, b: i32): i32 {
    return a + b;
}

fn test(): void {
    let result: i32 = helper(10);  // Error: wrong argument count
}
```

---

### 6. Wrong Argument Type
**Error Message:** `Argument <N> type mismatch in function call '<name>': expected '<type1>', got '<type2>'`  
**Location:** Line and column of the argument  
**Suggestion:** `Use a <type1> value for argument <N>`

**Example:**
```heidic
fn helper(a: i32, b: i32): i32 {
    return a + b;
}

fn test(): void {
    let result: i32 = helper("string", 20);  // Error: wrong argument type
}
```

---

### 7. If Condition Must Be Bool
**Error Message:** `If condition must be bool, got '<type>'`  
**Location:** Line and column of the if statement  
**Suggestion:** `Use a boolean expression: if (condition == true) or if (x > 0)`

**Example:**
```heidic
fn test(): void {
    let x: i32 = 10;
    if x {  // Error: if condition must be bool
        print("x is truthy");
    }
}
```

---

### 8. While Condition Must Be Bool
**Error Message:** `While condition must be bool, got '<type>'`  
**Location:** Line and column of the while statement  
**Suggestion:** `Use a boolean expression: while (condition == true) or while (x > 0)`

**Example:**
```heidic
fn test(): void {
    let x: i32 = 10;
    while x {  // Error: while condition must be bool
        x = x - 1;
    }
}
```

---

### 9. For Loop Collection Must Be Query
**Error Message:** `For loop collection must be a query type, got '<type>'`  
**Location:** Line and column of the for statement  
**Suggestion:** `Use a query: for entity in query<Position, Velocity>`

**Example:**
```heidic
fn test(): void {
    let x: i32 = 10;
    for entity in x {  // Error: for loop collection must be query
        print("item");
    }
}
```

---

### 10. Arithmetic Operations Require Numeric Types
**Error Message:** `Arithmetic operations require numeric types, got '<type1>' and '<type2>'`  
**Location:** Line and column of the arithmetic operation  
**Suggestion:** `Use numeric types (i32, i64, f32, f64) for arithmetic operations`

**Example:**
```heidic
fn test(): void {
    let x: bool = true;
    let y: i32 = x + 5;  // Error: arithmetic requires numeric types
}
```

---

### 11. Logical Operations Require Bool Types
**Error Message:** `Logical operations require bool types, got '<type1>' and '<type2>'`  
**Location:** Line and column of the logical operation  
**Suggestion:** `Use bool types for logical operations (&&, ||)`

**Example:**
```heidic
fn test(): void {
    let x: i32 = 10;
    let y: bool = x && true;  // Error: logical operations require bool
}
```

---

### 12. Negation Requires Numeric Type
**Error Message:** `Negation requires numeric type, got '<type>'`  
**Location:** Line and column of the negation operator  
**Suggestion:** `Use a numeric type (i32, i64, f32, f64) for negation`

**Example:**
```heidic
fn test(): void {
    let x: bool = true;
    let y: i32 = -x;  // Error: negation requires numeric type
}
```

---

### 13. Not Operator Requires Bool Type
**Error Message:** `Not requires bool type, got '<type>'`  
**Location:** Line and column of the not operator  
**Suggestion:** `Use a bool type for logical not (!)`

**Example:**
```heidic
fn test(): void {
    let x: i32 = 10;
    let y: bool = !x;  // Error: not requires bool type
}
```

---

### 14. Index Operation Requires Array Type
**Error Message:** `Index operation requires array type, got '<type>'`  
**Location:** Line and column of the index operation  
**Suggestion:** `Use an array type: array[index]`

**Example:**
```heidic
fn test(): void {
    let x: i32 = 10;
    let y: i32 = x[0];  // Error: index operation requires array type
}
```

---

### 15. SOA Component Field Must Be Array
**Error Message:** `SOA component '<name>' field '<field>' must be an array type (use [Type] instead of Type)`  
**Location:** Line and column of the field declaration  
**Suggestion:** `Change '<field>: <type>' to '<field>: [<type>]'`

**Example:**
```heidic
component_soa BadSOA {
    x: f32,  // Error: SOA component field must be array
    y: [f32],
    z: [f32]
}
```

---

## Error Message Format

All errors now include:

1. **Error Header:** `Error at <file>:<line>:<column>:`
2. **Source Context:** 
   - Previous line (if available)
   - Current line with error
   - Caret pointing to error location
   - Next line (if available)
3. **Error Message:** Clear description of the problem
4. **Suggestion:** Helpful hint on how to fix the issue (prefixed with 💡)

## Example Error Output

```
Error at test.hd:18:23:
 17 | fn test_undefined_variable(): void {
 18 |     let result: i32 = undefined_var + 5;  // Error: undefined variable
                           ^^^^^^^^^^^^^
 19 | }

Undefined variable: 'undefined_var'
💡 Suggestion: Did you mean to declare it first? Use: let undefined_var: Type = value;
```

---

## Notes

- The compiler currently stops at the first error encountered during type checking
- All errors include source location information (line and column)
- Suggestions are provided for most error types to help developers fix issues quickly
- Error messages are designed to be clear and actionable

