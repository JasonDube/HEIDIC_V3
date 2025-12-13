# Simple CMake Example

This is a minimal example showing how to create and build your own CMake project.

## Files

- `CMakeLists.txt` - The build configuration file
- `main.cpp` - Simple C++ program
- `README.md` - This file!

## How to Build

```bash
# 1. Navigate to this directory
cd examples/cmake_example

# 2. Create build directory
mkdir build
cd build

# 3. Configure with CMake
cmake .. -G "MinGW Makefiles"

# 4. Build
cmake --build .

# 5. Run!
simple_example.exe
```

## What You're Learning

- How to create your own CMakeLists.txt
- How CMake builds C++ programs
- The basic workflow: configure → build → run

## Next Steps

- Modify `main.cpp` and rebuild to see changes
- Add more `.cpp` files and update CMakeLists.txt
- Try linking a library (like Vulkan)

