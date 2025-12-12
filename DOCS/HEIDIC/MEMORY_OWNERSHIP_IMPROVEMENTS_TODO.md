# Memory Ownership Semantics - Future Improvements TODO

> **Status:** Current implementation is production-ready (9.5/10). These improvements would close remaining gaps and reach 10/10.

---

## 🔴 HIGH PRIORITY (Quick Wins - High Value)

### 1. Assignment Propagation ⭐ **HIGHEST PRIORITY**
**Status:** 🔴 Not Started  
**Effort:** 2-3 hours  
**Impact:** ⭐⭐⭐ **Fixes common aliasing issue - prevents indirect bugs**

**Problem:**
```heidic
fn test(frame: FrameArena): [Vec3] {
    let positions = frame.alloc_array<Vec3>(100);
    let y = positions;  // ✅ Currently allowed
    return y;  // ❌ Should error (y is frame-scoped via assignment)
}
```

**Solution:**
- When assigning `let y = x;` and x is frame-scoped, mark y as frame-scoped too
- Chain it (z = y → z scoped)
- Fixes indirect returns

**Implementation:**
- In `check_statement` for `Let`, check if RHS is a frame-scoped variable
- If so, mark the new variable as frame-scoped
- Handle chained assignments

**Frontier Team:** "Common aliasing issue—could lead to indirect bugs. Fixes indirect returns."

---

### 2. Param/Call-Site Checks ⭐ **HIGH PRIORITY**
**Status:** 🔴 Not Started  
**Effort:** 2-4 hours  
**Impact:** ⭐⭐⭐ **Prevents lifetime escapes via function parameters**

**Problem:**
```heidic
fn helper(arr: [Vec3]): [Vec3] {
    return arr;  // Might return frame-scoped memory
}

fn test(frame: FrameArena): void {
    let positions = frame.alloc_array<Vec3>(100);
    let result = helper(positions);  // ❌ Should error (positions might escape)
}
```

**Solution:**
- In function calls, if argument is frame-scoped and function returns Type matching arg, error
- Error message: "May escape frame-scope via return"
- Or tag function signatures with "escapes args?" annotation

**Implementation:**
- In `check_expression` for `Call`, check if any argument is frame-scoped
- Check if function return type matches the frame-scoped argument type
- Report error if escape is possible

**Frontier Team:** "No checks for passing frame-scoped vars to funcs. Edges toward lifetime escapes."

---

## 🟡 MEDIUM PRIORITY (Important for Robustness)

### 3. Scope-Aware Tracking
**Status:** 🔴 Not Started  
**Effort:** 3-4 hours  
**Impact:** ⭐⭐ **More precise tracking, handles shadows/nests correctly**

**Problem:**
```heidic
fn test(frame: FrameArena): void {
    let positions = frame.alloc_array<Vec3>(100);
    {
        let positions = Vec::new();  // Shadowing
        return positions;  // Should be valid (heap-allocated)
    }
}
```

**Solution:**
- Use stack of HashSets (push/pop on blocks)
- Handles nested scopes (if/loop blocks) precisely
- Remove variables from tracking when they go out of scope
- Handle variable shadowing correctly

**Implementation:**
- Replace `frame_scoped_vars: HashSet` with `frame_scoped_vars: Vec<HashSet>` (scope stack)
- Push new HashSet when entering block/if/loop
- Pop HashSet when exiting block/if/loop
- Check current scope stack for frame-scoped variables

**Frontier Team:** "Function-level tracking ignores nests (e.g., if/loop scopes)—could miss/misflag shadowed vars in blocks."

---

### 4. Type-Based Detection
**Status:** 🔴 Not Started  
**Effort:** 1-2 hours  
**Impact:** ⭐⭐ **More robust to code changes, less brittle**

**Problem:**
```heidic
fn test(my_frame: FrameArena): void {  // Renamed parameter
    let positions = my_frame.alloc_array<Vec3>(100);  // ❌ Current detection breaks
}
```

**Solution:**
- Instead of string-matching "alloc_array" + "frame" var, check expression type against FrameArena + method
- More robust to renames (e.g., renamed FrameArena parameter breaks current detection)

**Implementation:**
- In `is_frame_alloc_expression`, check if object expression type is `FrameArena`
- Check if member is "alloc_array" method
- Use type information instead of variable name matching

**Frontier Team:** "String-matching is simple but brittle (e.g., renamed FrameArena param breaks it). Type-based would be sturdier."

---

### 5. Expression Propagation
**Status:** 🔴 Not Started  
**Effort:** 2-3 hours  
**Impact:** ⭐⭐ **Catches more edge cases**

**Problem:**
```heidic
fn helper(arr: [Vec3]): [Vec3] {
    return arr;
}

fn test(frame: FrameArena): [Vec3] {
    return helper(frame.alloc_array<Vec3>(100));  // ❌ Should error
}
```

**Solution:**
- Track through expressions (e.g., `return foo(frame.alloc_array<>());` if foo returns its arg)
- Handle nested function calls that might propagate frame-scoped values

**Implementation:**
- In `check_expression` for `Call`, track if any argument is frame-scoped
- If function returns a type matching the frame-scoped argument, mark return as frame-scoped
- Propagate frame-scoped tracking through expression trees

**Frontier Team:** "Doesn't track through exprs (e.g., `return foo(frame.alloc_array<>());` if foo returns its arg)."

---

## 🟢 LOW PRIORITY (Polish & Advanced Features)

### 6. Global/Struct Escape Checks
**Status:** 🔴 Not Started  
**Effort:** 4-6 hours  
**Impact:** ⭐ **Catches additional escape paths**

**Problem:**
```heidic
static global_positions: [Vec3];

fn test(frame: FrameArena): void {
    let positions = frame.alloc_array<Vec3>(100);
    global_positions = positions;  // ❌ Should error (storing in global)
}
```

**Solution:**
- Flag assignments to globals/struct fields if RHS is frame-scoped
- Prevent storing frame-scoped variables in persistent storage

**Frontier Team:** "Misses other escapes (e.g., storing in globals/structs)—but doc justifies as 'most common case.'"

---

### 7. Error Message Polish
**Status:** 🔴 Not Started  
**Effort:** 1 hour  
**Impact:** ⭐ **Improves developer understanding**

**Enhancement:**
- Add note: "Note: FrameArena frees at scope end—use after return is UB."
- Enhance error messages with more context about why this is unsafe

**Frontier Team:** "Add 'Note: FrameArena frees at scope end—use after return is UB.'"

---

### 8. Tests Expansion
**Status:** 🔴 Not Started  
**Effort:** 1-2 hours  
**Impact:** ⭐ **Ensures robustness**

**Test Cases to Add:**
- [ ] Indirect assigns: `let y = positions; return y;`
- [ ] Shadows: `let positions = ...; { let positions = heap...; return positions; }`
- [ ] Nested scopes: frame-scoped vars in if/loop blocks
- [ ] Param escapes: `foo(positions)` where foo might return/store it
- [ ] Array element escapes: `return [frame.alloc_array<>(), heap_var];`
- [ ] Multi-function chains: `return foo(frame.alloc_array<>());`
- [ ] Renamed FrameArena parameter: `my_frame.alloc_array<>()`

**Frontier Team:** "Doc lacks explicit tests—add units for direct/indirect returns, shadows, etc."

---

## Implementation Priority

### Phase 1: Quick Wins (1 day)
1. ✅ Assignment Propagation (2-3 hours)
2. ✅ Param/Call-Site Checks (2-4 hours)

**Total:** ~6-7 hours - Closes most remaining gaps

### Phase 2: Robustness (1-2 days)
3. ✅ Scope-Aware Tracking (3-4 hours)
4. ✅ Type-Based Detection (1-2 hours)
5. ✅ Expression Propagation (2-3 hours)

**Total:** ~6-9 hours - Makes detection more robust

### Phase 3: Polish (1 day)
6. ✅ Error Message Polish (1 hour)
7. ✅ Tests Expansion (1-2 hours)
8. ✅ Global/Struct Escape Checks (4-6 hours) - Optional

**Total:** ~6-9 hours - Polish and edge cases

---

*Last updated: After frontier team evaluation*  
*Next milestone: Assignment Propagation (highest priority quick win)*

