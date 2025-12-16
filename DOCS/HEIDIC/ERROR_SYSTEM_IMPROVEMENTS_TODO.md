# Error System Improvements TODO

Based on comprehensive evaluation feedback, this document tracks all improvements to the HEIDIC compiler error reporting system.

## Priority: High Impact Quick Wins

### 1. Add Error Codes ⭐⭐⭐
**Status:** Pending  
**Effort:** 2 hours  
**Impact:** Huge usability boost

- [ ] Define error code enum/constants (E0001, E0002, etc.)
- [ ] Map each error type to a unique code
- [ ] Update error messages to include codes: `error[E0001]: ...`
- [ ] Create error code reference documentation
- [ ] Update ERROR_TYPES.md with codes

**Example:**
```
error[E0001]: undefined variable `undefined_var`
error[E0002]: type mismatch in assignment
error[E0003]: undefined function `unknown_function`
```

---

### 2. Collect Multiple Errors Per Pass ⭐⭐⭐
**Status:** Pending  
**Effort:** 1-2 days  
**Impact:** Saves developer time significantly

- [ ] Modify type checker to continue after first error
- [ ] Collect all errors in a pass instead of bailing
- [ ] Display error summary: "found 3 errors in test.hd"
- [ ] Sort errors by file/line number
- [ ] Limit to 20-30 errors before stopping (prevent spam)
- [ ] Update error reporter to handle multiple errors

**Example Output:**
```
error: found 3 errors in test.hd

error[E0001]: undefined variable `x`
  --> test.hd:5:10
   |
 5 |     y = x + 1;
   |         ^ not found in this scope

error[E0008]: if condition must be bool
  --> test.hd:8:8
   |
 8 |     if y {
   |        ^ got 'i32', expected 'bool'

error[E0002]: type mismatch in assignment
  --> test.hd:10:9
   |
10 |     y = "string";
   |         ^^^^^^^^ expected 'i32', got 'string'
```

---

### 3. Implement Fuzzy Matching for Undefined Identifiers ⭐⭐
**Status:** Pending  
**Effort:** 4-6 hours  
**Impact:** Great UX improvement

- [ ] Implement Levenshtein distance algorithm
- [ ] Search symbol table for close matches (distance ≤ 2)
- [ ] Show "Did you mean?" suggestions
- [ ] Handle variable name typos
- [ ] Handle function name typos
- [ ] Show location of suggested identifier if found

**Example:**
```
error[E0001]: undefined variable `postion`
  --> test.hd:18:23
   |
18 |     let result: f32 = postion.x;
   |                       ^^^^^^^ not found in this scope
   |
   = help: did you mean `position`? (declared at line 12)
```

---

## Priority: Medium Impact Polish

### 4. Add Verbosity Flags ⭐⭐
**Status:** Pending  
**Effort:** 1 hour  
**Impact:** Better for experienced developers

- [ ] Add `--quiet` flag: Error type and location only
- [ ] Add `--compact` flag: Terse output (Rust-style)
- [ ] Add `--verbose` flag: Extended help, type info, similar identifiers
- [ ] Default: Current format (good for beginners)
- [ ] Update CLI argument parsing

**Flags:**
- `--quiet`: Minimal output
- `--compact`: Concise Rust-style format
- `--verbose`: Maximum detail with type information

---

### 5. Add Error Severity Levels ⭐⭐
**Status:** Pending  
**Effort:** 2-3 hours  
**Impact:** Professional compiler feel

- [ ] Define severity enum: `error`, `warning`, `note`, `help`
- [ ] Categorize existing errors appropriately
- [ ] Add color coding (red/yellow/blue/green)
- [ ] Use terminal color library (if available)
- [ ] Update error reporter to support severity
- [ ] Add `--warn-as-error` flag

**Severity Levels:**
- `error`: Compilation cannot continue
- `warning`: Suspicious code, might be a bug
- `note`: Additional context
- `help`: Suggestion for fixing

---

### 6. Improve Suggestion Quality ⭐⭐
**Status:** Pending  
**Effort:** 4-6 hours  
**Impact:** Better developer experience

- [ ] Make suggestions more specific and actionable
- [ ] Show similar identifiers in scope for undefined variables
- [ ] Suggest imports for undefined functions
- [ ] Provide exact code fixes where possible
- [ ] Add "Note:" sections for common patterns
- [ ] Improve SOA error messages with context

**Examples:**
```
// Current:
💡 Suggestion: Did you mean to declare it first? Use: let undefined_var: Type = value;

// Improved:
💡 Did you mean 'undefined_value'? (declared at line 12)
💡 Or declare it: let undefined_var: Type = value;
```

---

## Priority: Missing Error Types

### 7. Add Return Type Mismatch Errors ⭐⭐⭐
**Status:** Pending  
**Effort:** 2-3 hours

- [ ] Check function return type matches declared type
- [ ] Error when returning wrong type
- [ ] Error when non-void function has no return
- [ ] Error when void function returns a value
- [ ] Suggest correct return type

**Example:**
```
error[E0016]: return type mismatch
  --> test.hd:5:5
   |
 5 |     return "string";
   |     ^^^^^^^^^^^^^^^ expected 'i32', got 'string'
```

---

### 8. Add Missing Return Statement Errors ⭐⭐
**Status:** Pending  
**Effort:** 1-2 hours

- [ ] Check all code paths return a value for non-void functions
- [ ] Error when function has no return statement
- [ ] Error when some paths don't return
- [ ] Suggest adding return statement

---

### 9. Add Duplicate Declaration Errors ⭐⭐
**Status:** Pending  
**Effort:** 2 hours

- [ ] Check for duplicate variable declarations in same scope
- [ ] Check for duplicate function declarations
- [ ] Check for duplicate component/struct declarations
- [ ] Show location of original declaration
- [ ] Suggest renaming or removing duplicate

---

### 10. Add Invalid Break/Continue Errors ⭐
**Status:** Pending  
**Effort:** 1 hour

- [ ] Error when `break` used outside loop
- [ ] Error when `continue` used outside loop
- [ ] Suggest using `return` instead if in function

---

### 11. Add Void Value Used Errors ⭐⭐
**Status:** Pending  
**Effort:** 1-2 hours

- [ ] Error when trying to use void function result
- [ ] Error when assigning void to variable
- [ ] Suggest removing assignment or using return value

---

### 12. Add Comparison Type Mismatch Errors ⭐⭐⭐
**Status:** Pending  
**Effort:** 2 hours

- [ ] Error when comparing incompatible types (e.g., `i32 == "string"`)
- [ ] Check `==`, `!=`, `<`, `>`, `<=`, `>=` operators
- [ ] Suggest explicit type conversion if appropriate

**Example:**
```
error[E0017]: comparison type mismatch
  --> test.hd:8:8
   |
 8 |     if x == "hello" {
   |        ^^^^^^^^^^^^ cannot compare 'i32' with 'string'
```

---

### 13. Add Component Access Errors ⭐⭐
**Status:** Pending  
**Effort:** 2-3 hours

- [ ] Error when accessing non-existent component field
- [ ] Error when accessing field on non-component type
- [ ] Suggest correct field name (fuzzy matching)
- [ ] Show available fields for component

---

### 14. Add Query Syntax Errors ⭐⭐
**Status:** Pending  
**Effort:** 2-3 hours

- [ ] Error for malformed query types
- [ ] Error when query contains non-component types
- [ ] Error when component in query doesn't exist
- [ ] Suggest correct query syntax

---

### 15. Add Array Index Type Errors ⭐
**Status:** Pending  
**Effort:** 1 hour

- [ ] Error when using non-integer as array index
- [ ] Error when index is wrong type
- [ ] Suggest using integer index

---

### 16. Add Division by Zero Detection (Static Analysis) ⭐
**Status:** Pending  
**Effort:** 2-3 hours

- [ ] Detect literal zero in division
- [ ] Warn about potential division by zero
- [ ] Suggest adding zero check

---

### 17. Add Unreachable Code Warnings ⭐
**Status:** Pending  
**Effort:** 2-3 hours

- [ ] Detect code after return statement
- [ ] Detect code after break/continue
- [ ] Warn about unreachable code
- [ ] Suggest removing or restructuring

---

## Format Improvements

### 18. Improve Error Message Format (Rust-style) ⭐⭐
**Status:** Pending  
**Effort:** 3-4 hours

- [ ] Adopt Rust-style compact format
- [ ] Use `-->` for file locations
- [ ] Use `=` for help/suggestions
- [ ] Improve caret positioning
- [ ] Add line number formatting

**Target Format:**
```
error[E0001]: undefined variable `x`
  --> test.hd:5:10
   |
 5 |     y = x + 1;
   |         ^ not found in this scope
   |
   = help: declare it first: let x: Type = value;
```

---

### 19. Add Color Coding ⭐⭐
**Status:** Pending  
**Effort:** 2-3 hours

- [ ] Red for errors
- [ ] Yellow for warnings
- [ ] Blue for notes
- [ ] Green for suggestions/help
- [ ] Bold for error codes and file paths
- [ ] Use terminal color library (e.g., `colored` crate)
- [ ] Auto-detect terminal color support
- [ ] Add `--no-color` flag

---

### 20. Enhanced SOA Error Messages ⭐
**Status:** Pending  
**Effort:** 1 hour

- [ ] Add context about SOA layout benefits
- [ ] Explain why arrays are required
- [ ] Show exact fix with before/after

**Example:**
```
error[E0015]: SOA component 'Velocity' field 'x' must be an array type
  --> test.hd:12:5
   |
12 |     x: f32,
   |     ^^^^^^ expected '[f32]', got 'f32'
   |
   = help: Change 'x: f32' to 'x: [f32]' to enable vectorized/SOA layout
   = note: SOA (Structure of Arrays) layout improves cache performance
```

---

### 21. Add "Note:" Sections for Common Patterns ⭐
**Status:** Pending  
**Effort:** 2 hours

- [ ] Add notes for common misconceptions
- [ ] Explain language-specific behavior
- [ ] Provide context for why error occurred

**Example:**
```
error[E0007]: if condition must be bool, got 'i32'
  --> test.hd:8:8
   |
 8 |     if x {
   |        ^ expected 'bool', got 'i32'
   |
   = help: use a comparison: if x != 0 { ... }
   = note: In HEIDIC, non-zero integers are not implicitly true (unlike C)
```

---

## Documentation

### 22. Update Error Documentation ⭐
**Status:** Pending  
**Effort:** 1 hour

- [ ] Add error codes to ERROR_TYPES.md
- [ ] Create error code reference guide
- [ ] Add examples for each error code
- [ ] Document verbosity flags
- [ ] Add troubleshooting guide

---

## Testing

### 23. Create Error Test Suite ⭐⭐
**Status:** Pending  
**Effort:** 3-4 hours

- [ ] Create test file for each error type
- [ ] Verify error messages are correct
- [ ] Test multiple error collection
- [ ] Test fuzzy matching accuracy
- [ ] Test verbosity flags
- [ ] Test color output (if enabled)

---

## Summary

**Total Estimated Effort:** ~2-3 weeks of focused work

**Priority Breakdown:**
- **High Priority (Quick Wins):** ~1 week
  - Error codes
  - Multiple errors
  - Fuzzy matching
  - Verbosity flags

- **Medium Priority (Polish):** ~1 week
  - Severity levels
  - Color coding
  - Format improvements
  - Better suggestions

- **Low Priority (Missing Types):** ~1 week
  - Additional error types
  - Static analysis warnings
  - Enhanced messages

**Current Status:** Error system is production-ready. These improvements will make it exceptional.

---

## Notes

- All improvements maintain backward compatibility
- Error codes make errors searchable and documentable
- Multiple error collection significantly improves developer experience
- Fuzzy matching reduces frustration from typos
- Color coding improves readability (when terminal supports it)
- Verbosity flags accommodate different user skill levels

