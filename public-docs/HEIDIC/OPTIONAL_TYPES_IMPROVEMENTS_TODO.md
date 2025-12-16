# Optional Types - Future Improvements TODO

> **Status:** Current implementation is production-ready for basic use cases (9.7/10, B+). These improvements would make it complete and reach A+.

---

## 🔴 CRITICAL FIXES (Required Before Shipping)

### 1. Type Compatibility Direction Verification ⭐ **CRITICAL - HIGHEST PRIORITY**
**Status:** 🔴 Not Started  
**Effort:** 1 hour  
**Impact:** ⭐⭐⭐ **Fixes correctness bug - prevents unsafe implicit unwrapping**

**Problem:**
```heidic
let opt: ?i32 = 42;
let value: i32 = opt;  // ❌ Should this be allowed? (Should NOT be!)
```

**Solution:**
- Verify that implicit unwrapping is NOT allowed
- Ensure `Optional(inner)` is NOT compatible with `actual` (non-optional)
- Only allow `actual` compatible with `Optional(inner)` (implicit wrapping)
- Test that assigning optional to non-optional produces a compile-time error

**Implementation:**
- Review `types_compatible()` in `src/type_checker.rs`
- Ensure the rule is: `actual` compatible with `Optional(inner)` (wrap a value into an optional) ✅
- NOT: `Optional(inner)` compatible with `actual` (implicitly unwrap—that's unsafe!) ❌
- Add test cases to verify this behavior

**Frontier Team:** "Wait—that second rule seems backwards. You want: `actual` compatible with `Optional(inner)` (wrap a value into an optional). Not: `Optional(inner)` compatible with `actual` (implicitly unwrap—that's unsafe!). Double-check this. If you can assign `?i32` to `i32` without explicit unwrap, that defeats the purpose of optional types."

---

### 2. Null Type Handling in Return Statements ⭐ **HIGH PRIORITY**
**Status:** 🔴 Not Started  
**Effort:** 1-2 hours  
**Impact:** ⭐⭐ **Fixes correctness bug - ensures null returns work correctly**

**Problem:**
```heidic
fn maybe_return(): ?i32 {
    if condition {
        return null;  // What type is this null?
    }
    return 42;
}
```

**Solution:**
- Use function return type context to determine null type
- Add explicit handling for `null` in return statements
- Verify that `null` in return statements works correctly

**Implementation:**
- In `check_statement()` for `Return`, check if the expression is `Literal::Null`
- If so, use the function's return type (from `current_function_return_type`) to determine the optional type
- Verify that `null` can be returned from functions with optional return types
- Add test cases to verify this behavior

**Frontier Team:** "How does the type checker know that `null` should be `?i32` here? The function return type provides context, but I don't see explicit handling for this in the document. Worth verifying this case works."

---

### 3. Nested Optionals Edge Case ⭐ **MEDIUM PRIORITY**
**Status:** 🔴 Not Started  
**Effort:** 1 hour  
**Impact:** ⭐ **Prevents confusion and edge case bugs**

**Problem:**
```heidic
let x: ??i32 = null;  // Optional of optional?
```

**Solution:**
- Explicitly handle or reject nested optionals (e.g., `??i32`)
- Add clear error message if nested optionals are not supported
- Document behavior if nested optionals are supported

**Implementation:**
- In `parse_type()`, detect nested optionals (e.g., `??Type`)
- Either reject with clear error: "Nested optionals are not supported. Use `?Type` instead."
- Or support nested optionals and document behavior (e.g., `unwrap()` returns `?i32`)
- Add test cases to verify this behavior

**Frontier Team:** "What happens with nested optionals? Is this allowed? If so, what does `unwrap()` return—a `?i32`? If not, is there an error? The document doesn't address this edge case."

---

### 4. Unwrap on Non-Optional Error Message ⭐ **MEDIUM PRIORITY**
**Status:** 🔴 Not Started  
**Effort:** 1 hour  
**Impact:** ⭐ **Ensures clear error messages**

**Problem:**
```heidic
let x: i32 = 42;
let y = x.unwrap();  // Should error with clear message
```

**Solution:**
- Verify error message is clear and helpful
- Ensure error message includes the variable name and type
- Add suggestion for how to fix the error

**Implementation:**
- Review error message in `check_expression()` for `MemberAccess` with `unwrap()`
- Ensure message is: "Cannot call unwrap() on non-optional type 'i32'. Variable 'x' has type 'i32', not an optional type."
- Add suggestion: "Use an optional type: let x: ?i32 = 42; or remove unwrap() if not needed."
- Add test cases to verify error message

**Frontier Team:** "Make sure this produces a clear, helpful error: 'unwrap() can only be called on optional types, but 'x' has type 'i32''"

---

## 🟡 HIGH PRIORITY (Important Enhancements)

### 5. Flow-Sensitive Type Checking ⭐ **HIGHEST PRIORITY ENHANCEMENT**
**Status:** 🔴 Not Started  
**Effort:** 1-2 days  
**Impact:** ⭐⭐⭐ **Achieves true null safety - prevents runtime crashes**

**Problem:**
```heidic
let mesh: ?Mesh = load_mesh("model.obj");
draw(mesh.unwrap());  // ❌ Compiles fine, crashes at runtime if mesh is null
```

**Solution:**
- Track null state through control flow
- Require null check before unwrap
- Warn about potential null dereference

**Implementation:**
- Add `null_state: HashMap<String, bool>` to `TypeChecker` to track which variables are known to be non-null
- In `check_statement()` for `If`, if condition is optional type, mark variable as non-null in then block
- In `check_expression()` for `MemberAccess` with `unwrap()`, check if variable is known to be non-null
- If not, report error: "Cannot unwrap() optional that may be null. Check for null first: if variable { variable.unwrap(); }"
- Add test cases to verify this behavior

**Frontier Team:** "This is a well-designed feature that gets the fundamentals right. However, the lack of flow-sensitive null checking means you haven't achieved true null safety. The value of optional types in Rust/Swift isn't just the syntax—it's that the compiler *forces* you to handle the null case. Flow-sensitive null checking (High impact on safety)."

---

### 6. Safe Unwrap Alternatives ⭐ **HIGH PRIORITY**
**Status:** 🔴 Not Started  
**Effort:** 2-3 hours  
**Impact:** ⭐⭐⭐ **Improves ergonomics - very common pattern**

**Enhancement:**
- Add `unwrap_or(default)` method
- Add `unwrap_or_else(closure)` method
- Add `expect(message)` method

**Implementation:**
- In `check_expression()` for `MemberAccess`, handle `unwrap_or`, `unwrap_or_else`, `expect`
- Validate that object is optional type
- Validate argument types (default value, closure, message)
- In `generate_expression()`, generate `.value_or(default)`, `.value_or_else(closure)`, `.value_or_throw(message)`
- Add test cases to verify this behavior

**Frontier Team:** "Add `unwrap_or()` quickly—it's a 2-hour feature that covers 80% of the ergonomic complaints. If I were prioritizing the 'future improvements,' I'd reorder: Flow-sensitive null checking (High impact on safety), `unwrap_or(default)` (Very common pattern, 2-hour fix)."

---

### 7. Null Coalescing Operator ⭐ **MEDIUM PRIORITY**
**Status:** 🔴 Not Started  
**Effort:** 1-2 hours  
**Impact:** ⭐⭐ **Syntactic sugar for unwrap_or**

**Enhancement:**
- Add `??` operator for default values
- Parse and generate null coalescing code

**Implementation:**
- Add `NullCoalesce` variant to `BinaryOp` enum
- In `parse_expression()`, handle `??` operator
- In `check_expression()`, validate that left side is optional type
- In `generate_expression()`, generate: `left ? left.value() : right`
- Add test cases to verify this behavior

**Frontier Team:** "`??` null coalescing (Syntactic sugar for unwrap_or)"

---

### 8. Optional Chaining
**Status:** 🔴 Not Started  
**Effort:** 2-4 hours  
**Impact:** ⭐⭐ **Nice but if-else works**

**Enhancement:**
- Add `?.` operator for method calls
- Add `?.` operator for property access
- Generate safe navigation code

**Frontier Team:** "Optional chaining `?.` (Nice but if-else works)"

---

## 🟢 MEDIUM PRIORITY (Nice-to-Have Features)

### 9. Pattern Matching for Optionals
**Status:** 🔴 Not Started  
**Effort:** 2-3 hours  
**Impact:** ⭐ **Depends on destructuring being done first**

**Enhancement:**
- Add `Some(value)` pattern
- Add `None` pattern
- Generate match code for optionals

**Frontier Team:** "Pattern matching `Some`/`None` (Depends on destructuring being done first)"

---

### 10. Implicit Wrapping Everywhere
**Status:** 🔴 Not Started  
**Effort:** 1 hour  
**Impact:** ⭐ **Improves ergonomics**

**Enhancement:**
- Allow implicit wrapping in assignments
- Allow implicit wrapping in function arguments
- Allow implicit wrapping in return values

---

## Implementation Priority

### Phase 1: Critical Fixes (1 day)
1. ✅ Type Compatibility Direction Verification (1 hour)
2. ✅ Null Type Handling in Return Statements (1-2 hours)
3. ✅ Nested Optionals Edge Case (1 hour)
4. ✅ Unwrap on Non-Optional Error Message (1 hour)

**Total:** ~4-5 hours - Fixes correctness bugs and ensures edge cases work

### Phase 2: High Priority Enhancements (2-3 days)
5. ✅ Flow-Sensitive Type Checking (1-2 days)
6. ✅ Safe Unwrap Alternatives (2-3 hours)
7. ✅ Null Coalescing Operator (1-2 hours)

**Total:** ~2-3 days - Achieves true null safety and improves ergonomics

### Phase 3: Medium Priority Features (1 day)
8. ✅ Optional Chaining (2-4 hours)
9. ✅ Pattern Matching for Optionals (2-3 hours)
10. ✅ Implicit Wrapping Everywhere (1 hour)

**Total:** ~5-8 hours - Adds advanced features

---

*Last updated: After frontier team evaluation (9.7/10, B+)*  
*Next milestone: Type Compatibility Verification + Flow-Sensitive Null Checking (critical fixes to achieve true null safety)*

