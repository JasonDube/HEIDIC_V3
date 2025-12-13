# Bouncing Balls - Hot-Reload Test Case

This project tests **all three types of hot-reload**:
1. **System Hot-Reload** - Edit movement patterns
2. **Shader Hot-Reload** - Edit visual appearance  
3. **Component Hot-Reload** - Add/remove component fields

## Quick Start

1. **Create the project in SCRIBE editor**:
   - Click the `+` button
   - Name it: `bouncing_balls`
   - The project will be created with the template code

2. **Replace the template code** with the code from `bouncing_balls.hd`

3. **Compile shaders**:
   - Make sure `shaders/ball.vert` and `shaders/ball.frag` exist
   - Click the blue "Compile Shaders" button (▶) in the editor

4. **Build and Run**:
   - Click the white "Run" button (▶)
   - You should see 5 colored cubes bouncing around!

## Testing Hot-Reload

### System Hot-Reload
1. Edit the `@hot system(movement)` functions
2. Change movement patterns (random, orbit, etc.)
3. Save and click the red "Hotload" button
4. See movement change instantly!

### Shader Hot-Reload
1. Edit `shaders/ball.frag` (change colors)
2. Click "Compile Shaders" (blue button)
3. Changes appear instantly!

### Component Hot-Reload (Coming Soon)
1. Edit `@hot component Position` or `Velocity`
2. Add/remove fields (e.g., `size: f32 = 1.0`)
3. Rebuild - component migration will run automatically!

## Current Status

✅ Basic rendering (5 balls as cubes)  
✅ System hot-reload infrastructure (ready to test)  
✅ Shader hot-reload infrastructure (ready to test)  
🟡 Component hot-reload (foundation ready, needs entity storage)  

## Next Steps

- Implement entity storage system
- Connect movement system to ECS queries
- Test component field additions/removals
- Add more balls and visual effects

