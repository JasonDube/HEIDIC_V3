# CUDA/OptiX Interop - Future Improvements TODO

> **Status:** Current implementation is a prototype (8.5/10, D+/C-). These improvements are required to make it shippable.

---

## 🔴 CRITICAL FIXES (Required Before Shipping)

### 1. Fill Transfer Placeholders ⭐ **CRITICAL - HIGHEST PRIORITY**
**Status:** 🔴 Not Started  
**Effort:** 2-3 hours  
**Impact:** ⭐⭐⭐ **Fixes non-functional code - makes it compilable**

**Problem:**
```cpp
cudaMemcpy(d_position_x, /* host_ptr */, sizeof(float) * /* size */, cudaMemcpyHostToDevice);
// Generated code won't compile due to placeholders
```

**Solution:**
- Extract arrays from queries (use registry offsets)
- Generate full memcpy for each field
- Handle H2D pre-launch, D2H post
- Implement query-to-kernel parameter mapping (the *hard* part)

**Implementation:**
- Extract query size from `query.entity_count()` or `query.size()`
- Extract host pointers from query object structure
- Generate complete memory transfer code for each SOA field
- Map query parameters to kernel parameters (e.g., `q.positions.x` → `float* position_x`)

**Frontier Team:** "The placeholders make this unusable. This isn't 'partially complete'—this is **non-functional**. The generated code won't compile. You can't ship code that produces uncompilable output and call it 'complete.'"

---

### 2. Loop Transformation ⭐ **CRITICAL - HIGHEST PRIORITY**
**Status:** 🔴 Not Started  
**Effort:** 3-4 hours  
**Impact:** ⭐⭐⭐ **Fixes core transformation - makes kernels actually work**

**Problem:**
```heidic
for entity in q {
    entity.Position.x += entity.Velocity.x * dt;
}
// How does this become thread indexing?
```

**Solution:**
- Transform `for entity in q` to thread indexing
- Eliminate loops and replace with `int idx = blockIdx.x * blockDim.x + threadIdx.x`
- Map entity access to array indexing

**Implementation:**
- Detect `for entity in q` loops in CUDA kernel functions
- Replace loop with thread indexing: `int idx = blockIdx.x * blockDim.x + threadIdx.x; if (idx >= count) return;`
- Transform `entity.Component.field` to `component_field[idx]`
- Handle SOA vs AoS access patterns

**Frontier Team:** "The Loop Transformation Isn't Shown. The `for entity in q` loop must be *eliminated* and replaced with thread indexing. Is this transformation implemented? The document doesn't show it."

---

### 3. Error Handling Basics ⭐ **CRITICAL**
**Status:** 🔴 Not Started  
**Effort:** 1-2 hours  
**Impact:** ⭐⭐⭐ **Fixes silent failures - makes debugging possible**

**Problem:**
```cpp
cudaMalloc(&d_x, size);  // What if this fails?
kernel<<<blocks, threads>>>(...);  // What if launch fails?
// CUDA calls fail silently by default
```

**Solution:**
- Add `cudaGetLastError` checks post-malloc/launch/memcpy
- Generate macros for debug asserts/logs
- Check `cudaMalloc`, kernel launch, and `cudaMemcpy` results

**Implementation:**
- Generate `cudaError_t err = cudaMalloc(...); if (err != cudaSuccess) { fprintf(stderr, "CUDA malloc failed: %s\n", cudaGetErrorString(err)); return; }`
- Add `cudaDeviceSynchronize()` after kernel launch
- Check `cudaGetLastError()` after each CUDA call
- Generate error messages with context

**Frontier Team:** "CUDA calls fail silently by default. Without error checking, your users will spend hours debugging silent failures. You need `cudaError_t err = cudaMalloc(&d_x, size); if (err != cudaSuccess) { fprintf(stderr, "CUDA malloc failed: %s\n", cudaGetErrorString(err)); return; }`"

---

### 4. Persistent Device Memory ⭐ **CRITICAL**
**Status:** 🔴 Not Started  
**Effort:** 3-4 hours  
**Impact:** ⭐⭐⭐ **Fixes performance killer - makes it usable in game loops**

**Problem:**
```cpp
void update_physics_launch(...) {
    cudaMalloc(&d_x, ...);  // Allocate
    // ... use ...
    cudaFree(d_x);  // Free
}
// Allocating and freeing on every call is extremely slow
```

**Solution:**
- Pre-allocate device memory (not malloc/free per call)
- Reuse allocations across frames
- Use memory pools

**Implementation:**
- Generate global device memory pointers (persistent across calls)
- Allocate once at initialization, free at shutdown
- Track allocation sizes and reuse if size matches
- Generate memory pool or reuse logic

**Frontier Team:** "Allocating and freeing device memory on every kernel launch is extremely slow. Real implementations: Pre-allocate device memory, reuse allocations across frames, use memory pools. This pattern would kill performance in a real game loop."

---

## 🟡 HIGH PRIORITY (Important Enhancements)

### 5. OptiX Stub ⭐ **HIGH PRIORITY**
**Status:** 🔴 Not Started  
**Effort:** 3-4 hours  
**Impact:** ⭐⭐ **Completes the feature name - makes OptiX actually work**

**Enhancement:**
- Add `@[optix(raygen = name)]` for RT progs
- Generate OptixProgramGroup, build pipelines
- Start with raygen/miss/hit

**Frontier Team:** "OptiX is Mentioned But Not Implemented. Including headers isn't implementation. This should be called 'CUDA Codegen (OptiX Planned)' to be accurate."

---

### 6. Dynamic Sizing ⭐ **HIGH PRIORITY**
**Status:** 🔴 Not Started  
**Effort:** 1 hour  
**Impact:** ⭐⭐ **Fixes kernel launch - makes it work with real queries**

**Enhancement:**
- Pull `n` from `query.entity_count()`
- Generate adaptive blocks/grids

**Frontier Team:** "No Query Size Extraction. Kernel launch configuration uses placeholder `/* size */` instead of actual query size."

---

### 7. Device Query ⭐ **MEDIUM PRIORITY**
**Status:** 🔴 Not Started  
**Effort:** 1-2 hours  
**Impact:** ⭐ **Improves robustness - handles edge cases**

**Enhancement:**
- Check for CUDA device availability
- Check device memory capacity

**Frontier Team:** "What if there's no CUDA device? What if the device doesn't have enough memory? The generated code assumes CUDA is available and has infinite memory."

---

### 8. Stream/Async ⭐ **MEDIUM PRIORITY**
**Status:** 🔴 Not Started  
**Effort:** 2-3 hours  
**Impact:** ⭐⭐ **Improves performance - enables async execution**

**Enhancement:**
- Add `--stream` param to launch
- Generate `cudaStream_t` + async memcpy/launch

**Frontier Team:** "This works but is slow. For real applications you need: Async copies with streams, double buffering, overlap of compute and transfer."

---

## 🟢 MEDIUM PRIORITY (Nice-to-Have Features)

### 9. Kernel Validation
**Status:** 🔴 Not Started  
**Effort:** 4-6 hours  
**Impact:** ⭐ **Improves safety - catches invalid kernel code**

**Enhancement:**
- Scan for invalid ops (e.g., printf in kernel)
- Warn at check-time

---

### 10. Unified Memory Support
**Status:** 🔴 Not Started  
**Effort:** 2-3 hours  
**Impact:** ⭐ **Simplifies memory management**

**Enhancement:**
- Add unified memory allocation
- Generate unified memory code

---

### 11. Error Polish
**Status:** 🔴 Not Started  
**Effort:** 1 hour  
**Impact:** ⭐ **Improves error messages**

**Enhancement:**
- For bad attrs: Suggest "Use @[cuda] only on SOA components"

---

## Implementation Priority

### Phase 1: Critical Fixes (1 week)
1. ✅ Fill Transfer Placeholders (2-3 hours)
2. ✅ Loop Transformation (3-4 hours)
3. ✅ Error Handling Basics (1-2 hours)
4. ✅ Persistent Device Memory (3-4 hours)

**Total:** ~9-13 hours - Makes it compilable and functional

### Phase 2: High Priority Enhancements (1 week)
5. ✅ OptiX Stub (3-4 hours)
6. ✅ Dynamic Sizing (1 hour)
7. ✅ Device Query (1-2 hours)
8. ✅ Stream/Async (2-3 hours)

**Total:** ~7-10 hours - Completes the feature and improves performance

### Phase 3: Medium Priority Features (1 week)
9. ✅ Kernel Validation (4-6 hours)
10. ✅ Unified Memory Support (2-3 hours)
11. ✅ Error Polish (1 hour)

**Total:** ~7-10 hours - Adds polish and safety

---

*Last updated: After frontier team evaluation (8.5/10, D+/C-)*  
*Next milestone: Fill Transfer Placeholders + Loop Transformation + Error Handling (critical fixes to make it compilable and functional)*

