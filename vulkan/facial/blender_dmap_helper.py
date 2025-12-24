"""
Blender Script: DMap Helper
============================
Helps create DMap textures from sculpted poses in Blender.

Usage:
1. Create base head mesh with UV0 and UV1
2. Duplicate mesh and sculpt extreme pose
3. Run this script to bake displacement to DMap texture
"""

import bpy
import bmesh
from mathutils import Vector
import numpy as np

def create_dmap_from_sculpt(base_obj_name, sculpt_obj_name, uv_map_index=1, output_path="dmap.png", resolution=1024):
    """
    Create a DMap texture by comparing base mesh to sculpted mesh.
    
    Args:
        base_obj_name: Name of base mesh object
        sculpt_obj_name: Name of sculpted mesh object
        uv_map_index: UV map index to use (0=UV0, 1=UV1)
        output_path: Where to save the DMap PNG
        resolution: Texture resolution (power of 2, e.g., 1024, 2048)
    """
    
    # Get objects
    base_obj = bpy.data.objects.get(base_obj_name)
    sculpt_obj = bpy.data.objects.get(sculpt_obj_name)
    
    if not base_obj or not sculpt_obj:
        print(f"Error: Objects not found. Base: {base_obj_name}, Sculpt: {sculpt_obj_name}")
        return False
    
    # Get meshes
    base_mesh = base_obj.data
    sculpt_mesh = sculpt_obj.data
    
    # Check UV maps
    if len(base_mesh.uv_layers) <= uv_map_index:
        print(f"Error: Base mesh needs UV map at index {uv_map_index}")
        return False
    
    uv_layer = base_mesh.uv_layers[uv_map_index]
    
    # Initialize texture (RGB, 0-255)
    texture = np.zeros((resolution, resolution, 3), dtype=np.uint8)
    texture.fill(128)  # Neutral gray (no displacement)
    
    # Create bmesh for base mesh
    bm_base = bmesh.new()
    bm_base.from_mesh(base_mesh)
    bm_base.faces.ensure_lookup_table()
    bm_base.verts.ensure_lookup_table()
    
    # Create bmesh for sculpted mesh
    bm_sculpt = bmesh.new()
    bm_sculpt.from_mesh(sculpt_mesh)
    bm_sculpt.verts.ensure_lookup_table()
    
    # Get world matrices
    base_matrix = base_obj.matrix_world
    sculpt_matrix = sculpt_obj.matrix_world
    
    # Sample each vertex
    for face in bm_base.faces:
        for loop in face.loops:
            vert_idx = loop.vert.index
            uv = loop[uv_layer]
            
            # Get base position (world space)
            base_pos = base_matrix @ loop.vert.co
            
            # Get sculpted position (world space)
            if vert_idx < len(bm_sculpt.verts):
                sculpt_pos = sculpt_matrix @ bm_sculpt.verts[vert_idx].co
            else:
                sculpt_pos = base_pos  # No corresponding vertex
            
            # Calculate displacement delta
            delta = sculpt_pos - base_pos
            
            # Convert UV to pixel coordinates
            u = int(uv.uv.x * (resolution - 1))
            v = int((1.0 - uv.uv.y) * (resolution - 1))  # Flip V
            u = max(0, min(resolution - 1, u))
            v = max(0, min(resolution - 1, v))
            
            # Convert displacement to RGB (normalize to reasonable range)
            # Assuming max displacement of 0.1 units (10cm)
            max_displacement = 0.1
            normalized = delta / max_displacement
            normalized = np.clip(normalized, -1.0, 1.0)
            
            # Map -1..1 to 0..255
            r = int((normalized.x + 1.0) * 127.5)
            g = int((normalized.y + 1.0) * 127.5)
            b = int((normalized.z + 1.0) * 127.5)
            
            # Write to texture (average if multiple samples hit same pixel)
            texture[v, u, 0] = min(255, texture[v, u, 0] + (r - 128) // 2)
            texture[v, u, 1] = min(255, texture[v, u, 1] + (g - 128) // 2)
            texture[v, u, 2] = min(255, texture[v, u, 2] + (b - 128) // 2)
    
    # Save texture
    try:
        from PIL import Image
        img = Image.fromarray(texture, 'RGB')
        img.save(output_path)
        print(f"Saved DMap to: {output_path}")
        return True
    except ImportError:
        print("Error: PIL (Pillow) not installed. Install with: pip install Pillow")
        return False
    
    finally:
        bm_base.free()
        bm_sculpt.free()


# Example usage (uncomment and modify):
# create_dmap_from_sculpt("Head_Base", "Head_Sculpt_JawOpen", uv_map_index=1, output_path="jaw_open_dmap.png")

