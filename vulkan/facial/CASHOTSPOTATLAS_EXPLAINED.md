# CASHotSpotAtlas Explained

## What is CASHotSpotAtlas?

CASHotSpotAtlas is Sims 4's UV1 layout system. It's a **specialized UV map that only covers the head/face**, organized by facial regions.

## Key Points:

### 1. **It's ONLY for the head/face**
- The body uses regular UV0 (or no UV1)
- Only facial vertices have UV1 coordinates
- Body vertices typically have UV1 = (0, 0) or are ignored

### 2. **It's islands packed into a single texture atlas**
- Each facial feature becomes its own UV island (brow island, lip island, etc.)
- These islands are packed into ONE texture atlas
- Different islands occupy different designated areas of that texture
- Think of it like a "control panel" with labeled sections

### 3. **Region-based layout (not appearance-based)**
- UV0: Organized to minimize seams, maximize texture quality
- UV1: Organized by FUNCTION (lips together, brows together, etc.)

## Visual Example:

```
┌─────────────────────────────────────────────────────────────┐
│                    YOUR FULL BODY MODEL                      │
│                                                               │
│  ┌─────────────┐                                            │
│  │    HEAD     │ ← Has UV1 coordinates (facial regions)     │
│  │             │                                             │
│  └─────────────┘                                             │
│       │                                                       │
│       │                                                       │
│  ┌─────────────┐                                            │
│  │    BODY     │ ← No UV1 (or UV1 = 0,0)                    │
│  │             │                                             │
│  └─────────────┘                                             │
└─────────────────────────────────────────────────────────────┘

UV0 (Skin Texture):
┌─────────────────────────────────────┐
│  [Head unwrapped]                   │  ← Optimized for texture quality
│  [Body unwrapped]                   │
│  [Arms, legs, etc.]                 │
└─────────────────────────────────────┘

UV1 (DMap Atlas - HEAD ONLY):
┌─────────────────────────────────────┐
│  ┌─────────┐                        │
│  │ BROW    │ ← Brow island packed here
│  │ ISLAND  │   (0.4-0.7, 0.7-1.0)
│  └─────────┘                        │
│                                      │
│  ┌─────────┐  ┌─────────┐          │
│  │  NOSE   │  │  LIP    │          │
│  │ ISLAND  │  │ ISLAND   │          │
│  └─────────┘  └─────────┘          │
│  (0.0-0.6,    (0.6-0.9,             │
│   0.4-0.7)     0.3-0.5)             │
│                                      │
│         ┌─────────┐                 │
│         │  JAW    │                 │
│         │ ISLAND  │                 │
│         └─────────┘                 │
│         (0.7-1.0, 0.0-0.3)          │
│                                      │
│  [Safe Region]                       │  ← Body vertices at (0.1, 0.1)
│  (0.0-0.1, 0.0-0.1)                 │
└─────────────────────────────────────┘

Each feature is its own ISLAND, packed into designated regions!
```

## How It Works:

### Step 1: UV1 Layout Creation (in Blender)
```
1. Select HEAD/FACE faces
2. Create second UV layer (UV1)
3. Unwrap head/face → creates islands (one per feature)
4. Manually pack islands into designated regions:
   - Select BROW island → pack into (0.4-0.7, 0.7-1.0) region
   - Select LIP island → pack into (0.6-0.9, 0.3-0.5) region
   - Select JAW island → pack into (0.7-1.0, 0.0-0.3) region
   - Select NOSE island → pack into (0.0-0.6, 0.4-0.7) region
   - etc.
5. Select BODY faces → Set UV1 to (0.1, 0.1) for all (safe region)
```

**Key Point:** Each facial feature becomes its own island, then you pack those islands into specific atlas regions!

### Step 2: DMap Painting
```
When you paint a DMap texture:
- Paint in the "Lip Region" (0.6-0.9, 0.3-0.5) → affects only lips
- Paint in the "Brow Region" (0.4-0.7, 0.7-1.0) → affects only brows
- Paint in the "Jaw Region" (0.7-1.0, 0.0-0.3) → affects only jaw
```

### Step 3: Runtime
```
Shader samples DMap:
- Lip vertex with UV1 = (0.75, 0.4) → samples DMap at that location
- Gets displacement vector → moves lip vertex
- Body vertex with UV1 = (0, 0) → samples DMap at (0, 0) → gets no displacement
```

## Why This Design?

### Advantages:
1. **Efficient**: One texture for all facial expressions
2. **Organized**: Easy to paint specific facial features
3. **Flexible**: Can blend multiple expressions by painting in different regions
4. **Performance**: Only head needs UV1, body is ignored

### Example Use Case:
```
Want a "smile" expression?
→ Paint displacement in Lip Region (0.6-0.9, 0.3-0.5)
→ Paint displacement in Cheek Region (0.0-0.6, 0.4-0.7)
→ Only those regions deform, rest of face stays neutral
```

## For Your Full Body Model:

**Current approach (copying UV0):**
- ✅ Works for testing
- ✅ All vertices get UV1 (head + body)
- ⚠️ Body vertices will also sample DMap (might cause unwanted deformation)

**Better approach (Sims 4 style):**
- Create UV1 that only maps the HEAD
- Body vertices get UV1 = (0, 0) or are in a "safe" region
- DMap texture has "safe" (black/no displacement) region at (0, 0)
- Only head deforms, body stays static

## Summary:

**CASHotSpotAtlas = UV1 layout where:**
- ✅ Only HEAD/face has meaningful UV1 coordinates
- ✅ Facial regions are organized into specific atlas areas
- ✅ It's ONE texture atlas (not separate islands)
- ✅ Body vertices map to a "safe" region (no displacement)

**For testing:** Copying UV0 works fine!
**For production:** Create a head-only UV1 with region-based layout.

