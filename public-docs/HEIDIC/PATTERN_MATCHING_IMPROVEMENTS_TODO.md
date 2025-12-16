# Pattern Matching - Future Improvements TODO

> **Status:** Current implementation is production-strong for basic use cases (9.4/10, A-/B+). These improvements would make it complete and reach A+.

---

## 🔴 CRITICAL FIXES (Required Before Shipping)

### 1. Expression Evaluation Order Fix ⭐ **CRITICAL - HIGHEST PRIORITY**
**Status:** 🔴 Not Started  
**Effort:** 1-2 hours  
**Impact:** ⭐⭐⭐ **Fixes correctness bug - prevents side effects from executing multiple times**

**Problem:**
```heidic
match get_value() {
    0 => { ... }
    1 => { ... }
}
// Generated: if (get_value() == 0) { ... } else if (get_value() == 1) { ... }
// ❌ get_value() is called twice!
```

**Solution:**
- Generate temporary variable to store expression result
- Use temporary in all arm conditions
- Example: `auto __match_tmp = get_value(); if (__match_tmp == 0) { ... }`

**Implementation:**
- In `generate_expression` for `Match`, check if expression has side effects
- Generate: `auto __match_tmp = <expr>;` before if-else chain
- Use `__match_tmp` in all arm conditions instead of re-evaluating expression

**Frontier Team:** "The expression re-evaluation bug is the main issue that should be fixed before shipping—it's a correctness problem, not just a quality-of-life thing."

---

### 2. Variable Binding Declaration Fix ⭐ **CRITICAL - HIGHEST PRIORITY**
**Status:** 🔴 Not Started  
**Effort:** 1-2 hours  
**Impact:** ⭐⭐⭐ **Fixes correctness bug - ensures generated C++ compiles**

**Problem:**
```heidic
match result {
    value => { print(value); }  // ❌ 'value' not declared in C++
}
// Generated: if ((value = result, true)) { print(value); }
// ❌ 'value' is not declared!
```

**Solution:**
- Generate variable declaration for pattern bindings
- Use `auto var_name = expr;` before if-else chain
- Example: `auto value = result; if (true) { /* arm body */ }`

**Implementation:**
- In `generate_expression` for `Match`, detect variable binding patterns
- Generate: `auto <var_name> = <expr>;` before if-else chain
- Use `var_name` directly in arm body (no need for comma operator)

**Frontier Team:** "In C++, this assumes `var_name` is already declared. If not, you need to emit something like: `auto value = result; if (true) { /* arm body */ }`"

---

### 3. Wildcard Position Enforcement ⭐ **HIGH PRIORITY**
**Status:** 🔴 Not Started  
**Effort:** 1 hour  
**Impact:** ⭐⭐ **Prevents dead code and confusion**

**Problem:**
```heidic
match x {
    _ => { print("catch all"); }
    42 => { print("never reached"); }  // ❌ Dead code!
}
```

**Solution:**
- Validate that wildcard patterns are last arm
- Warn about unreachable arms after wildcard
- Add check in parser or type checker

**Implementation:**
- In `check_expression` for `Match`, iterate through arms
- If wildcard found, check if it's the last arm
- If not, report warning: "Wildcard pattern should be last arm. Arms after wildcard are unreachable."

**Frontier Team:** "This should be enforced or at least warned. A simple check in the parser or type checker would catch this."

---

## 🟡 HIGH PRIORITY (Important Enhancements)

### 4. Match as Expression ⭐ **HIGH PRIORITY**
**Status:** 🔴 Not Started  
**Effort:** 2-4 hours  
**Impact:** ⭐⭐⭐ **Enables functional-style code and inline matching**

**Problem:**
```heidic
let result = match x {
    0 => 1,
    1 => 2,
    _ => 3
};  // ❌ Not supported - match doesn't return values
```

**Solution:**
- Support match expressions that return values
- Infer return type from all arm bodies
- Ensure all arms return compatible types
- Generate ternary chains or immediately-invoked lambda

**Frontier Team:** "This is the right call for a first implementation. Match-as-expression requires all arms to have the same return type, exhaustiveness checking, and different codegen."

---

### 5. Exhaustiveness Checking Basics ⭐ **HIGH PRIORITY**
**Status:** 🔴 Not Started  
**Effort:** 3-4 hours  
**Impact:** ⭐⭐⭐ **Prevents bugs from missing cases**

**Problem:**
```heidic
match result {
    VK_SUCCESS => { ... }
    // Missing VK_ERROR_OUT_OF_MEMORY, etc.
    // No error - should warn about non-exhaustive match
}
```

**Solution:**
- Check if all enum variants are covered
- Check if bool patterns are exhaustive
- Warn about non-exhaustive matches
- Use component/enum registry for variant tracking

**Frontier Team:** "For enums/known types, warn if arms miss cases (e.g., via registry metadata). Add `_` req for defaults."

---

### 6. Identifier vs Variable Disambiguation ⭐ **HIGH PRIORITY**
**Status:** 🔴 Not Started  
**Effort:** 1-2 hours  
**Impact:** ⭐⭐ **Prevents confusing behavior**

**Problem:**
```heidic
match result {
    value => { ... }  // Variable binding?
    VK_SUCCESS => { ... }  // Identifier pattern?
}
// How does parser distinguish?
```

**Solution Options:**
- **Option A:** Uppercase convention (Rust-like): uppercase = identifier, lowercase = variable
- **Option B:** Symbol table lookup: check if identifier exists as constant/enum variant
- **Option C:** Explicit syntax: `@VK_SUCCESS` for identifiers, `value` for variables

**Implementation:**
- Choose disambiguation strategy
- Update parser to distinguish based on chosen strategy
- Document behavior in implementation report

**Frontier Team:** "This needs to be explicit, or you'll get confusing behavior. Is it uppercase = identifier, lowercase = variable? Check symbol table for existing constants?"

---

### 7. Float Literal Comparison Warning ⭐ **MEDIUM PRIORITY**
**Status:** 🔴 Not Started  
**Effort:** 1 hour  
**Impact:** ⭐⭐ **Prevents floating point equality bugs**

**Problem:**
```heidic
match value {
    3.14 => { ... }  // ❌ Generates: value == 3.14 (floating point equality)
}
```

**Solution Options:**
- **Option A:** Add warning when matching on float literals
- **Option B:** Disallow float literal patterns
- **Option C:** Document exact bitwise comparison behavior

**Implementation:**
- In `check_pattern`, detect float literal patterns
- Report warning: "Floating point equality comparison is unreliable. Consider using variable binding and range check."
- Or disallow float literal patterns entirely

**Frontier Team:** "Floating point equality comparison is almost always wrong. Consider: warning when matching on floats, or don't support float literal patterns at all, or document that this is exact bitwise comparison."

---

## 🟢 MEDIUM PRIORITY (Nice-to-Have Features)

### 8. Pattern Guards Addition
**Status:** 🔴 Not Started  
**Effort:** 2-3 hours  
**Impact:** ⭐⭐ **Improves expressiveness**

**Enhancement:**
- Add `if` conditions to patterns: `value if value > 10 => { ... }`
- Parse and validate guard expressions
- Generate guard conditions in codegen: `if (match && cond)`

**Frontier Team:** "Extend arms to `pattern if cond =>`—parse cond expr, gen `if (match && cond)`."

---

### 9. Destructuring Lite
**Status:** 🔴 Not Started  
**Effort:** 4-6 hours  
**Impact:** ⭐⭐ **Enables more powerful patterns**

**Enhancement:**
- Support struct destructuring: `Point { x, y } => { ... }`
- Use component registry for field offsets
- Parse and validate destructuring patterns

**Frontier Team:** "For structs, `Struct { field } =>`—use registry offsets to bind. Ties into reflection."

---

### 10. Range Patterns
**Status:** 🔴 Not Started  
**Effort:** 2 hours  
**Impact:** ⭐ **Improves ergonomics**

**Enhancement:**
- Support range patterns: `1..=10 => { ... }`
- Parse range syntax
- Generate range checks in codegen: `>= && <=`

**Frontier Team:** "`1..=10 =>`—parse range, gen >= && <=."

---

## Implementation Priority

### Phase 1: Critical Fixes (1 day)
1. ✅ Expression Evaluation Order Fix (1-2 hours)
2. ✅ Variable Binding Declaration Fix (1-2 hours)
3. ✅ Wildcard Position Enforcement (1 hour)

**Total:** ~3-5 hours - Fixes correctness bugs and prevents dead code

### Phase 2: High Priority Enhancements (1-2 days)
4. ✅ Match as Expression (2-4 hours)
5. ✅ Exhaustiveness Checking Basics (3-4 hours)
6. ✅ Identifier vs Variable Disambiguation (1-2 hours)
7. ✅ Float Literal Comparison Warning (1 hour)

**Total:** ~7-11 hours - Adds essential features and prevents bugs

### Phase 3: Medium Priority Features (1-2 days)
8. ✅ Pattern Guards Addition (2-3 hours)
9. ✅ Destructuring Lite (4-6 hours)
10. ✅ Range Patterns (2 hours)

**Total:** ~8-11 hours - Adds advanced features

---

*Last updated: After frontier team evaluation (9.4/10, A-/B+)*  
*Next milestone: Expression Evaluation Order Fix + Variable Binding Declaration Fix (critical fixes to reach A+)*

