# DDS Format Selection Guide

## Quick Decision Tree

```
Do you need alpha/transparency?
├─ YES → DXT5 or BC7 (both support full alpha)
│         └─ Prefer BC7 if targeting modern hardware
│
└─ NO → DXT1, DXT5, or BC7
          ├─ Maximum quality → BC7
          ├─ Balance quality/size → DXT5  
          └─ Maximum compression → DXT1
```

## Recommendations by Texture Type

| Texture Type | Recommended Format | Alternative | Notes |
|-------------|-------------------|-------------|-------|
| **Albedo/Diffuse** | **BC7** | DXT5 | Best quality for color |
| **Normal Maps** | **BC5** | DXT5 | BC5 is 2-channel optimized |
| **Roughness/Metalness** | **R8** or **BC4** | DXT1 | Single-channel, grayscale |
| **Emissive** | **R8** or **BC7** | DXT5 | Single-channel if grayscale |
| **UI/Sprites (with alpha)** | **BC7** | DXT5 | Full alpha support |
| **Skybox/Cubemaps** | **BC7** | DXT5 | High quality needed |

## Quality Comparison (Subjective)

- **BC7**: ⭐⭐⭐⭐⭐ (Near-lossless)
- **DXT5**: ⭐⭐⭐⭐ (Good quality, slight artifacts on gradients)
- **DXT1**: ⭐⭐⭐ (Visible artifacts, especially on gradients)

## File Size Comparison (2048×2048 RGBA)

- **BC7**: ~1-2MB
- **DXT5**: ~2MB
- **DXT1**: ~1MB (no alpha) or ~1.3MB (1-bit alpha)

## Hardware Compatibility

- **BC7**: DirectX 11+ (2011+), Vulkan, OpenGL 4.2+ (most GPUs 2012+)
- **DXT5**: DirectX 9+ (2002+), Vulkan, OpenGL 1.3+ (ALL modern GPUs)
- **DXT1**: DirectX 6+ (1998+), Vulkan, OpenGL 1.3+ (ALL GPUs)

## For Testing the DDS Loader

**For your test right now, any format will work!** 

- If you want the best visual quality: **BC7**
- If you want maximum compatibility: **DXT5**
- If you just want to see it work quickly: **DXT1**

All three are supported by our loader and will display correctly.

## Converting with texconv (DirectXTex)

```bash
# BC7 (recommended for color textures)
texconv.exe -bc 7 source.png -o output.dds

# DXT5 (good balance, full alpha)
texconv.exe -bc 5 source.png -o output.dds

# DXT1 (maximum compression, limited alpha)
texconv.exe -bc 1 source.png -o output.dds

# BC5 (for normal maps - 2 channel)
texconv.exe -bc 5 -n 1 source.png -o output.dds
```

## Bottom Line

**For most use cases:** Use **BC7** for color textures and **BC5** for normal maps.

**For your test:** Any format works, but BC7 will look best!

