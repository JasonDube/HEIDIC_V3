# EDEN ENGINE Conventions & Standards

## Coordinate System

EDEN ENGINE uses a **Right-Handed, Y-Up** coordinate system. This is the standard mathematical convention used in engineering, physics, and many DCC tools like **Autodesk Maya**.

### Axes
- **+X**: Right
- **+Y**: Up
- **+Z**: Backward (Toward the viewer)
- **-Z**: Forward (Into the screen)

### Visualization
If you hold up your right hand:
- **Thumb** points Right (+X)
- **Index Finger** points Up (+Y)
- **Middle Finger** points toward you (+Z)

This convention is consistent with standard GLM (OpenGL Mathematics) usage.

---

## Vulkan Integration Details

While the engine logic uses Y-Up (Right-Handed), the Vulkan backend uses a different clip space (Y-Down, Z [0, 1]). The engine automatically handles this conversion in the **Projection Matrix**:

1.  **Depth (Z)**: We use `glm::perspectiveRH_ZO`. This maps the camera's Z range to Vulkan's **[0, 1]** depth range (Zero-to-One).
2.  **Vertical (Y)**: We flip the Y-axis (`proj[1][1] *= -1`) in the projection matrix. This compensates for Vulkan's Y-Down clip space, allowing you to continue reasoning in a Y-Up world.

**Key Takeaway**: You do NOT need to manually flip coordinates or UVs. Write your gameplay code assuming **Y is Up** and **Forward is -Z**.

---

## Units and Measurements

Unless otherwise specified, EDEN ENGINE uses metric units:
- **Distance**: Meters (1.0 = 1 meter)
- **Angle**: Radians (GLM functions expect radians)
- **Time**: Seconds

---

## Tool Compatibility

### Autodesk Maya
Maya defaults to **Y-Up, Right-Handed**. This matches EDEN ENGINE perfectly.
- **Exporting**: Models exported from Maya (e.g., as OBJ or glTF) should generally work without rotation fixes.
- **Camera**: A camera looking down -Z in Maya matches our camera conventions.

### Blender
Blender defaults to **Z-Up, Right-Handed**.
- **Exporting**: When exporting from Blender, ensure you select "Y-Up" (usually an option in glTF/OBJ exporters) or simply rotate your models -90 degrees on X to align them.

### Unity
Unity uses **Y-Up, Left-Handed**.
- **Difference**: The Z-axis is inverted relative to ours. +Z is Forward in Unity.
- **Porting**: Math logic involving Cross Products or forward vectors will need to be negated.

### Unreal Engine
Unreal uses **Z-Up, Left-Handed**.
- **Difference**: Both the Up axis and Handedness differ. Requires significant conversion (swapping Y/Z and inverting an axis).

