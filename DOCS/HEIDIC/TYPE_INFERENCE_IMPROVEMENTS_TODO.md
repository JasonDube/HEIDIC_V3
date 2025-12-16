# Type Inference Improvements - Future Improvements TODO

> **Status:** Current implementation is production-ready (9.6/10, B+/A-). These improvements would close remaining gaps and reach 10/10.

---

## 🔴 HIGH PRIORITY (Quick Wins - High Value)

### 1. Contextual Basics ⭐ **HIGHEST PRIORITY**
**Status:** 🔴 Not Started  
**Effort:** 2-3 hours  
**Impact:** ⭐⭐⭐ **Fixes empties and improves ergonomics**

**Problem:**
```heidic
fn process(arr: [i32]): void {
    let empty = [];  // ❌ Currently errors - can't infer type
}

fn test(): void {
    let numbers: [i32] = [];  // Must be explicit
}
```

**Solution:**
- Add top-down inference for assigns/params (e.g., `fn foo(arr: [i32]) { let x = []; }` → infer `[i32]`)
- Fixes empty arrays too
- Infer from function parameter types
- Infer from assignment targets

**Implementation:**
- In `check_statement` for `Let`, if value is empty array literal, check if target type is known
- In function calls, if argument is empty array, infer from parameter type
- Add bidirectional type checking for array literals

**Frontier Team:** "Add top-down for assigns/params. Fixes empties too."

---

### 2. Struct Codegen Completion ⭐ **HIGH PRIORITY**
**Status:** 🔴 Not Started  
**Effort:** 2-4 hours  
**Impact:** ⭐⭐⭐ **Completes struct literal feature**

**Problem:**
```heidic
struct Vec3 { x: f32, y: f32, z: f32 }
let pos = Vec3(x: 1.0, y: 2.0, z: 3.0);  // Parsed but not codegen'd
```

**Solution:**
- Finish codegen for struct literals
- Use inferred fields to validate/construct
- Ties into component registry for metadata
- Validate field names and types match struct definition

**Implementation:**
- Complete `generate_expression` for `StructLiteral`
- Validate fields against struct definition in type checker
- Generate C++ struct initialization code
- Handle default values if specified

**Frontier Team:** "Finish gen for literals—use inferred fields to validate/construct. Ties into registry for metadata."

**Critical Gap:** "That `fields: _` concerns me slightly. You're ignoring field validation entirely here—the type checker will say 'yep, that's a `Vec3`' without verifying the fields match the struct definition."

---

## 🟡 MEDIUM PRIORITY (Important for Robustness)

### 3. Inference Propagation
**Status:** 🔴 Not Started  
**Effort:** 3-4 hours  
**Impact:** ⭐⭐ **Handles indirect type propagation**

**Problem:**
```heidic
fn test(): void {
    let y = [1, 2];  // Infers [i32]
    let z = y;  // Should carry inferred type
    // Currently works, but ensure it propagates correctly
}
```

**Solution:**
- For chains (e.g., `let y = [1,2]; let z = y;`)—carry inferred types
- Handles indirects
- Ensure type information flows through assignments

**Frontier Team:** "For chains—carry inferred types. Handles indirects."

---

### 4. Nested Array Testing
**Status:** 🔴 Not Started  
**Effort:** 1 hour  
**Impact:** ⭐⭐ **Ensures correctness**

**Problem:**
```heidic
let matrix = [[1, 2], [3, 4]];  // Should infer [[i32]] (array of arrays)
// Not explicitly tested
```

**Solution:**
- Add explicit test cases for nested arrays
- Document behavior
- Verify recursive type checking works correctly

**Implementation:**
- Add test case: `let matrix = [[1, 2], [3, 4]];`
- Verify it infers `[[i32]]`
- Test with different element types
- Test with empty nested arrays

**Frontier Team:** "What happens with `let matrix = [[1, 2], [3, 4]];`? The implementation should handle this (since `check_expression` is recursive), but it's not tested in the examples. Worth adding a test case."

---

### 5. Type Compatibility Enhancement
**Status:** 🔴 Not Started  
**Effort:** 2-3 hours  
**Impact:** ⭐⭐ **More flexible numeric inference**

**Problem:**
```heidic
let mixed = [1, 2L];  // ❌ Error: i32 vs i64 mismatch (even though i32 can widen to i64)
```

**Solution:**
- Add numeric widening rules (i32 → i64, i32 → f32, etc.) with explicit opt-in
- Document strict equality behavior
- Allow mixing compatible numeric types with explicit cast suggestion

**Implementation:**
- Extend `types_compatible` to handle numeric widening
- Add opt-in flag for numeric coercion
- Update error messages to suggest casts when types are compatible but not equal

**Frontier Team:** "Does it handle numeric coercion (e.g., can you mix `i32` and `i64` with widening)? The document implies strict equality, which is fine for a first pass, but worth being explicit about."

---

### 6. Error Polish
**Status:** 🔴 Not Started  
**Effort:** 1 hour  
**Impact:** ⭐ **Improves developer experience**

**Enhancement:**
- For mixed types: Suggest casts ("Cast elem 3 to i32: (elem as i32)")
- For empties: "Infer from context or add elem"
- Improve error message clarity

**Frontier Team:** "For mixed: Suggest casts. For empties: 'Infer from context or add elem.'"

---

## 🟢 LOW PRIORITY (Polish & Advanced Features)

### 7. Codegen Error Validation
**Status:** 🔴 Not Started  
**Effort:** 1-2 hours  
**Impact:** ⭐ **Prevents invalid codegen**

**Problem:**
```rust
// In check_statement_with_return_type
// Reports errors but continues with Ok(())
// Potentially allows invalid code to proceed to codegen
```

**Solution:**
- Ensure codegen phase validates types before generating code
- Stop compilation after type checking phase if errors exist
- Add validation pass before codegen

**Frontier Team:** "The `check_statement_with_return_type` function reports the error but then continues with `Ok(())`. This is fine for error recovery (collect multiple errors), but make sure the compiler doesn't proceed to codegen with invalid types."

---

### 8. Fixed-Size Array Types
**Status:** 🔴 Not Started  
**Effort:** 1-2 days  
**Impact:** ⭐ **Enables fixed-size array inference**

**Problem:**
```heidic
let arr = [1, 2, 3];  // Always generates std::vector<int32_t>
// No way to specify fixed-size array like int32_t[3]
```

**Solution:**
- Add fixed-size array types (e.g., `[i32; 3]`)
- Track array length in type system
- Generate appropriate C++ code (std::array vs std::vector)

**Frontier Team:** "This generates C++ initializer list syntax, which works for `std::vector<T> v = {1, 2, 3};`. But does HEIDIC's type system track array length? If someone passes this to a function expecting a fixed-size array, there could be a mismatch."

---

## 🔵 BACK BURNER (Advanced Features - Future Consideration)

### 9. Generic Type Inference
**Status:** 🔴 Not Started  
**Effort:** 1-2 weeks  
**Impact:** High (requires generics feature first)

**Problem:**
```heidic
// If HEIDIC had generics:
fn bar<T>(arr: [T]): void {
    let x = [val];  // Can't infer T from val
}
```

**Solution:**
- When generics land, infer params (e.g., `fn bar<T>(arr: [T]) { let x = [val]; }` → T from val)
- Infer generic type parameters from usage
- Infer template arguments

**Frontier Team:** "When generics land, infer params (e.g., `fn bar<T>(arr: [T]) { let x = [val]; }` → T from val)."

---

### 10. Type Inference Diagnostics
**Status:** 🔴 Not Started  
**Effort:** 1 day  
**Impact:** Low (developer experience)

**Enhancement:**
- Show inferred types in error messages
- Add `--show-inferred-types` flag
- Help developers understand what types were inferred

---

### 11. Advanced Type Inference
**Status:** 🔴 Not Started  
**Effort:** 1-2 weeks  
**Impact:** Medium (complexity vs. benefit)

**Enhancement:**
- Handle unions/intersections if types diverge (e.g., infer supertype)
- Infer from multiple sources (bidirectional)
- Infer from type constraints

**Frontier Team:** "Handle unions/intersections if types diverge (e.g., infer supertype)."

---

## Implementation Priority

### Phase 1: Quick Wins (1 day)
1. ✅ Contextual Basics (2-3 hours)
2. ✅ Struct Codegen Completion (2-4 hours)

**Total:** ~4-7 hours - Closes most remaining gaps

### Phase 2: Robustness (1-2 days)
3. ✅ Nested Array Testing (1 hour)
4. ✅ Type Compatibility Enhancement (2-3 hours)
5. ✅ Inference Propagation (3-4 hours)
6. ✅ Error Polish (1 hour)

**Total:** ~7-9 hours - Makes inference more robust

### Phase 3: Polish (1 day)
7. ✅ Codegen Error Validation (1-2 hours)
8. ✅ Fixed-Size Array Types (1-2 days) - Optional

**Total:** ~1-2 days - Polish and edge cases

---

*Last updated: After frontier team evaluation (9.6/10, B+/A-)*  
*Next milestone: Contextual Basics + Struct Codegen Completion (quick wins to reach 10/10)*

