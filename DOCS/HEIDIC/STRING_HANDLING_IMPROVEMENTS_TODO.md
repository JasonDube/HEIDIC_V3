# String Handling Improvements - Future Improvements TODO

> **Status:** Current implementation is production-usable for numeric/bool embeds (9.3/10, C+/B-). These improvements would make it complete and reach A-.

---

## 🔴 CRITICAL FIXES (Required for Full Functionality)

### 1. String Variable Fix ⭐ **CRITICAL - HIGHEST PRIORITY**
**Status:** 🔴 Not Started  
**Effort:** 1 hour (Option A) or 2-3 hours (Option B)  
**Impact:** ⭐⭐⭐ **Fixes critical compile-time failure - enables string interpolation for strings**

**Problem:**
```heidic
let name: string = "Player";
let msg = "Hello, {name}!";  // ❌ Generated: std::to_string(name) - won't compile!
```

**Solution Option A (Recommended - Helper Function Strategy):**
1. Add `heidic::to_string()` helper function with overloads in C++ runtime header:
```cpp
namespace heidic {
    template <typename T>
    std::string to_string(T value) { return std::to_string(value); }
    
    std::string to_string(const std::string& value) { return value; }
    
    std::string to_string(bool value) { return value ? "true" : "false"; }
}
```
2. Update codegen to generate `heidic::to_string(var)` instead of `std::to_string(var)`
3. C++ function overloading handles type dispatch automatically

**Solution Option B (Type-Aware Codegen):**
1. Pass type information from type checker to codegen
2. In codegen, check variable type:
   - If `Type::String`: use variable directly (no conversion)
   - If numeric: use `std::to_string(var)`
   - If `Type::Bool`: use `(var ? "true" : "false")`

**Frontier Team:** "The string variable bug is a compile-time failure, not a runtime quirk. That's a critical gap. If string interpolation doesn't work with strings, you don't really have string interpolation yet."

---

### 2. Bool Pretty-Print Fix ⭐ **HIGH PRIORITY**
**Status:** 🔴 Not Started  
**Effort:** 1 hour (or included in Option A above)  
**Impact:** ⭐⭐⭐ **Fixes user-facing text output - critical for logs/UI**

**Problem:**
```heidic
let is_alive: bool = true;
let msg = "Player is {is_alive}";  // Outputs: "Player is 1" instead of "Player is true"
```

**Solution:**
- Generate `(var ? "true" : "false")` for bool types instead of `std::to_string(var)`
- Or use helper function strategy (handles both string and bool)

**Frontier Team:** "This is more serious than it sounds. If someone writes `'Active: {is_active}'` and gets `'Active: 1'`, that's going into user-facing text."

---

### 3. String Concatenation Operator ⭐ **HIGH PRIORITY**
**Status:** 🔴 Not Started  
**Effort:** 2-3 hours  
**Impact:** ⭐⭐ **Enables manual fallback when interpolation isn't suitable**

**Problem:**
```heidic
let greeting = "Hello, " + name;  // ❌ Not supported (arithmetic + only works for numeric types)
```

**Solution:**
- Add `BinaryOp::Concat` variant for string concatenation
- In parser, detect string + string or string + variable
- In type checker, validate both operands are strings
- In codegen, generate `s1 + s2` for string concatenation

**Frontier Team:** "Lacks string concat (`s1 + s2`)—forces interp or manual funcs. Feels incomplete for basics."

---

## 🟡 MEDIUM PRIORITY (Important Enhancements)

### 4. Format Specifiers Basics
**Status:** 🔴 Not Started  
**Effort:** 2-4 hours  
**Impact:** ⭐⭐ **Enables precision control for numeric output**

**Problem:**
```heidic
let score: f32 = 1234.567;
let msg = "Score: {score:.2f}";  // ❌ Format specifiers not supported
```

**Solution:**
- Extend interpolation syntax: `{value:.2f}`, `{value:04d}`, etc.
- Parse format specifiers in `parse_string_interpolation`
- Generate `std::format` (C++20) or `std::ostringstream` code
- Start with floats/ints

**Frontier Team:** "No `{health:.2f}`—limits precision, but back-burner appropriate for v1."

---

### 5. Nested/Escaped Braces
**Status:** 🔴 Not Started  
**Effort:** 1-2 hours  
**Impact:** ⭐⭐ **Enables literal braces in output**

**Problem:**
```heidic
let msg = "Use {{braces}} like this";  // ❌ Currently not handled
// Should output: "Use {braces} like this"
```

**Solution:**
- In `parse_string_interpolation`, handle `{{` → `{` escaping
- Treat `{{` as literal `{` in output
- Treat `}}` as literal `}` in output

**Frontier Team:** "What happens with `'Use {{braces}} like this'`? I don't see the implementation handling `{{` → `{` escaping."

---

### 6. Empty Interpolation Handling
**Status:** 🔴 Not Started  
**Effort:** 1 hour  
**Impact:** ⭐ **Prevents confusion**

**Problem:**
```heidic
let msg = "Hello, {}!";  // ❌ What does this produce? Should be an error.
```

**Solution:**
- In `parse_string_interpolation`, detect empty variable name `{}`
- Report error: "Empty variable name in string interpolation"

**Frontier Team:** "What does `'Hello, {}!'` produce? Is that an error, or does it output literally `{}`? Should probably be an error."

---

### 7. Codegen Efficiency Improvements
**Status:** 🔴 Not Started  
**Effort:** 2-3 hours  
**Impact:** ⭐ **Reduces temporary string allocations**

**Problem:**
```cpp
// Current: Creates multiple temporary std::string objects
std::string("Hello, ") + std::to_string(name) + std::string("! Health: ") + std::to_string(health)
```

**Solution Options:**
- Use `std::ostringstream` for better efficiency
- Or generate single `std::format` call (C++20)
- Or use `reserve` + `append` pattern

**Frontier Team:** "This creates multiple temporary `std::string` objects. Modern C++ optimizers handle this reasonably well, but if you wanted to be thorough..."

---

## 🟢 LOW PRIORITY (Standard Library Features)

### 8. String Manipulation Functions
**Status:** 🔴 Not Started  
**Effort:** 1-2 days  
**Impact:** ⭐ **Enables more string operations**

**Functions to Add:**
- `split(str: string, delimiter: string): [string]`
- `join(arr: [string], delimiter: string): string`
- `format(template: string, ...args): string`
- `trim(str: string): string`
- `to_lower(str: string): string`
- `to_upper(str: string): string`

**Frontier Team:** "Missing split/join/upper/format—std lib gap, but doc defers wisely (add as needed)."

---

### 9. Expressions in Interpolation
**Status:** 🔴 Not Started  
**Effort:** 3-5 days  
**Impact:** ⭐ **Enables more flexible interpolation**

**Problem:**
```heidic
let msg = "Health: {health + 10}";  // ❌ Expressions not supported
```

**Solution:**
- Allow expressions inside `{}`: `{health + 10}`, `{x * 2}`, etc.
- Parse expression inside braces
- Type check expression, ensure result is convertible to string
- Generate appropriate conversion code

**Frontier Team:** "Alphanum-only vars (no exprs like `{health + 10}`)—simple but restricts (your future has it)."

---

## Implementation Priority

### Phase 1: Critical Fixes (1 day)
1. ✅ String Variable Fix (1 hour - Option A recommended)
2. ✅ Bool Pretty-Print Fix (1 hour - or included in Option A)
3. ✅ String Concatenation Operator (2-3 hours)

**Total:** ~4-5 hours - Makes feature complete and shippable

### Phase 2: Polish (1 day)
4. ✅ Nested/Escaped Braces (1-2 hours)
5. ✅ Empty Interpolation Handling (1 hour)
6. ✅ Format Specifiers Basics (2-4 hours)

**Total:** ~4-7 hours - Adds polish and edge case handling

### Phase 3: Optimization (1 day)
7. ✅ Codegen Efficiency Improvements (2-3 hours)

**Total:** ~2-3 hours - Performance optimization

### Phase 4: Standard Library (Future)
8. ✅ String Manipulation Functions (1-2 days)
9. ✅ Expressions in Interpolation (3-5 days)

**Total:** ~1 week - Advanced features

---

*Last updated: After frontier team evaluation (9.3/10, C+/B-)*  
*Next milestone: String Variable Fix + Bool Pretty-Print Fix (critical fixes to reach A-)*

