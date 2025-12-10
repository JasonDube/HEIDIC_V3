# Better Error Messages - Implementation Plan

## Overview

Task 3 of Sprint 1: Improve error messages to be more helpful and developer-friendly.

**Current State:**
```
Error: Type mismatch in assignment
```

**Target State:**
```
Error at examples/test.hd:42:8:
    let x: f32 = "hello";
                 ^^^^^^^
Type mismatch: cannot assign 'string' to 'f32'
Suggestion: Use a float variable or convert: x = 10.0
```

---

## Implementation Steps

### Step 1: Add Source Location Tracking

**Goal:** Track line and column numbers for all AST nodes

**Changes:**
1. Create `SourceLocation` struct
2. Add location to AST nodes (optional, to avoid breaking existing code)
3. Update lexer to track line/column
4. Update parser to attach locations

**Files to Modify:**
- `src/ast.rs` - Add `SourceLocation` struct
- `src/lexer.rs` - Track line/column during tokenization
- `src/parser.rs` - Attach locations to AST nodes

### Step 2: Enhanced Error Reporting

**Goal:** Create error reporting system with context

**Changes:**
1. Create `ErrorReporter` struct
2. Store source file path
3. Read source lines for context
4. Format errors with location and context

**Files to Create/Modify:**
- `src/error.rs` - New error reporting module
- `src/main.rs` - Use error reporter

### Step 3: Multiple Error Collection

**Goal:** Collect all errors instead of stopping at first

**Changes:**
1. Change error handling to collect errors
2. Report all errors at end
3. Continue processing after errors (where possible)

**Files to Modify:**
- `src/type_checker.rs` - Collect errors instead of bailing
- `src/parser.rs` - Collect errors instead of bailing

---

## Implementation Order

1. **Step 1** - Source location tracking (foundation)
2. **Step 2** - Enhanced error reporting (display)
3. **Step 3** - Multiple error collection (polish)

---

*Let's start implementing!*

