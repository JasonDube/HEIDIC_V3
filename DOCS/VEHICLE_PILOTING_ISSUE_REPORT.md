# Vehicle Piloting System Issue Report

## Problem Summary
When piloting a vehicle using WASD keys, the player character strafes instead of rotating with the vehicle. The vehicle's logical rotation (yaw) updates correctly, but the player's position relative to the vehicle is not maintained properly.

## System Architecture

### Vehicle Setup
- **Blue Vehicle (Cube 14)**: A rectangular block (1 unit tall, 4 units wide, 10 units long)
- **Pink Helm (Cube 15)**: A 0.5×0.5×0.5 cube positioned at the front of the vehicle
- **Pilot Mode**: Activated by pressing 'P' when near the pink helm and having it selected

### Expected Behavior
1. **W key**: Move vehicle forward in its facing direction
2. **A key**: Rotate vehicle left (decrease yaw) - player should rotate WITH the vehicle
3. **D key**: Rotate vehicle right (increase yaw) - player should rotate WITH the vehicle
4. **S key**: Move vehicle backward
5. **Player**: Should maintain relative position on vehicle, rotating with it (no strafing)
6. **Pink Helm**: Should stay at the front of the vehicle (rotates with vehicle)

### Actual Behavior
1. **First press of A/D**: Player strafes to the side (incorrect)
2. **Subsequent presses**: Pink box rotates around vehicle's Y axis (correct)
3. **Vehicle**: Remains visually stationary (only logical rotation, no visual rotation)
4. **Player**: Strafe movement occurs instead of rotating with vehicle

## Technical Details

### Coordinate System
- Y-up coordinate system
- Vehicle yaw: 0° = facing +Z direction
- Vehicle dimensions: X=4 (width), Y=1 (height), Z=10 (length)
- Pink helm positioned at +Z edge (front) of vehicle

### Current Implementation

#### Offset System
- `vehicle_offset_x`, `vehicle_offset_z`: Player's relative position on vehicle (recalculated every frame)
- `locked_vehicle_offset_x`, `locked_vehicle_offset_z`: Locked offset when entering pilot mode
- Offset should rotate with vehicle yaw to maintain relative position

#### Player Position Update (Pilot Mode)
```heidic
// When vehicle rotates, rotate the player's locked offset
if yaw_delta != 0.0 {
    let cos_delta: f32 = heidic_cos(yaw_delta_rad);
    let sin_delta: f32 = heidic_sin(yaw_delta_rad);
    let rotated_offset_x: f32 = locked_vehicle_offset_x * cos_delta - locked_vehicle_offset_z * sin_delta;
    let rotated_offset_z: f32 = locked_vehicle_offset_x * sin_delta + locked_vehicle_offset_z * cos_delta;
    locked_vehicle_offset_x = rotated_offset_x;
    locked_vehicle_offset_z = rotated_offset_z;
}

// Update player position
camera_pos.x = new_rect_x + locked_vehicle_offset_x;
camera_pos.z = new_rect_z + locked_vehicle_offset_z;
```

#### Vehicle Rotation
```heidic
// A - Turn vehicle left
if glfwGetKey(window, 65) == 1 {  // GLFW_KEY_A
    vehicle_yaw = vehicle_yaw - vehicle_turn_speed * delta_time;
}

// D - Turn vehicle right
if glfwGetKey(window, 68) == 1 {  // GLFW_KEY_D
    vehicle_yaw = vehicle_yaw + vehicle_turn_speed * delta_time;
}
```

## Attempted Solutions

1. **Locked Offset System**: Created persistent `locked_vehicle_offset_*` variables that are set when entering pilot mode
2. **Disabled Player Movement**: Player movement (WASD) is disabled when `pilot_mode == 1`
3. **Offset Rotation**: Implemented rotation of locked offset when vehicle yaw changes
4. **Position Locking**: Player position is only updated in pilot mode section, not in normal movement code

## Current Issues

### Issue 1: Initial Strafing
- **Symptom**: First press of A/D causes player to strafe
- **Possible Cause**: Offset recalculation happening before locked offset is used, or timing issue with offset locking

### Issue 2: Vehicle Not Rotating Visually
- **Symptom**: Vehicle cube remains visually stationary
- **Note**: This might be expected - vehicle uses logical rotation (yaw affects movement direction), not visual rotation
- **Question**: Should the vehicle cube rotate visually, or is logical rotation sufficient?

### Issue 3: Offset Recalculation
- **Symptom**: `vehicle_offset_x` and `vehicle_offset_z` are recalculated every frame from camera position
- **Impact**: May interfere with locked offset system
- **Location**: Calculated at top of frame loop, before pilot mode section

## Code Flow

1. **Frame Start**: Calculate `vehicle_offset_x/z` from current camera position
2. **Normal Movement**: If not in pilot mode, update camera position with WASD
3. **Offset Update**: If on vehicle and not in pilot mode, update offset
4. **Pilot Mode**:
   - Update `vehicle_yaw` based on A/D keys
   - Rotate locked offset if yaw changed
   - Update player position using locked offset
   - Update vehicle position (if W/S pressed)
   - Update pink helm position (rotated based on yaw)

## Questions for Review

1. **Offset Calculation Timing**: Should `vehicle_offset_x/z` be recalculated every frame, or only when needed?
2. **Rotation Matrix**: Is the rotation matrix correct for rotating the offset?
   - Current: `x' = x*cos(θ) - z*sin(θ)`, `z' = x*sin(θ) + z*cos(θ)`
   - Is this the correct rotation for Y-axis rotation?
3. **Initial Offset Lock**: Is the offset being locked correctly when entering pilot mode?
4. **Visual vs Logical Rotation**: Should the vehicle cube rotate visually, or is logical rotation (affecting movement direction) sufficient?
5. **Frame Order**: Is the order of operations correct? Should offset rotation happen before or after position update?

## Environment

- **Language**: HEIDIC (custom language, transpiles to C++)
- **Rendering**: Vulkan
- **Input**: GLFW
- **Coordinate System**: Right-handed, Y-up

## Request for Help

We're looking for:
1. Review of the rotation matrix implementation
2. Suggestions for preventing initial strafing
3. Best practices for maintaining relative position on a rotating platform
4. Whether visual rotation of the vehicle is necessary or if logical rotation is sufficient

Any insights or alternative approaches would be greatly appreciated!

