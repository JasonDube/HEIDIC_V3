# Build System To-Do List

This document tracks build system improvements and fixes for HEIDIC_v2.

## Completed ✅

- [x] **FPS Camera Build Script** (Nov 30, 2025)
  - Status: ✅ Completed
  - Fixed batch script parsing errors with delayed expansion
  - Resolved variable scope loss after `endlocal`
  - Implemented automatic Vulkan SDK and GLFW detection
  - Created working build script for `fps_camera` example
  - Added comprehensive debug output for troubleshooting

- [x] **Batch Script Variable Management** (Nov 30, 2025)
  - Status: ✅ Completed
  - Restructured conditionals to use `goto` labels instead of nested `if/else`
  - Removed problematic `endlocal` in middle of script
  - All variables now use delayed expansion (`!VAR!`) throughout
  - Single `endlocal` at end for cleanup

- [x] **Path Handling** (Nov 30, 2025)
  - Status: ✅ Completed
  - Convert Windows backslashes to forward slashes for `g++` linking
  - Proper handling of paths with spaces and special characters

## In Progress 🚧

*(No build system tasks currently in progress)*

## Pending 📋

- [ ] **GCC ImGui Compilation Fix** (Nov 30, 2025)
  - **Priority**: Medium
  - **Issue**: GCC 15.2.0 crashes with segmentation fault when compiling ImGui with `-O3` optimization (AVX-512 intrinsics bug)
  - **Current Workaround**: Uses cached object files from previous builds
  - **Proposed Solutions**:
    1. Lower optimization for ImGui files (`-O2` or `-O1`)
    2. Skip ImGui compilation if object files already exist and are newer than source
    3. Use different GCC version or compiler
  - **Impact**: Build succeeds but ImGui changes require manual object file deletion

- [ ] **Build Script Template/Generator** (Nov 30, 2025)
  - **Priority**: Low
  - **Goal**: Create a template build script that can be easily copied for new examples
  - **Features**:
    - Automatic example name detection
    - Configurable optimization levels
    - Optional dependency inclusion (ImGui, GLFW, Vulkan)
  - **Benefit**: Faster example creation, consistent build process

- [ ] **Cross-Platform Build Support** (Nov 30, 2025)
  - **Priority**: Low
  - **Goal**: Support Linux and macOS builds in addition to Windows
  - **Requirements**:
    - Detect OS and use appropriate build commands
    - Handle different library paths (Linux: `/usr/lib`, macOS: Homebrew paths)
    - Support `pkg-config` for library detection
  - **Current Status**: Windows-only batch scripts

- [ ] **Incremental Build Support** (Nov 30, 2025)
  - **Priority**: Low
  - **Goal**: Only recompile changed files
  - **Implementation**:
    - Check file modification times
    - Skip compilation if object file is newer than source
    - Only link if any object files changed
  - **Benefit**: Faster iteration during development

- [ ] **Build Error Reporting** (Nov 30, 2025)
  - **Priority**: Low
  - **Goal**: Better error messages when builds fail
  - **Features**:
    - Clear indication of which step failed
    - Suggestions for common errors (missing libraries, wrong paths)
    - Link to relevant documentation
  - **Benefit**: Easier debugging for new users

- [ ] **Dependency Version Checking** (Nov 30, 2025)
  - **Priority**: Low
  - **Goal**: Verify that required dependencies are present and correct versions
  - **Checks**:
    - Vulkan SDK version
    - GLFW version
    - GCC version
    - Required system libraries
  - **Benefit**: Catch configuration issues early

- [ ] **Parallel Compilation** (Nov 30, 2025)
  - **Priority**: Low
  - **Goal**: Compile multiple source files in parallel
  - **Implementation**: Use `g++ -j` or batch job control
  - **Benefit**: Faster builds for projects with many source files

## Notes

- **PowerShell vs CMD**: User prefers PowerShell for terminal commands to avoid hangs. Batch scripts are used for build automation.
- **Debug Output**: Comprehensive debug output was crucial for identifying build script issues. Consider making it optional with a `--verbose` flag.
- **Path Variables**: Environment variables (`VULKAN_SDK`, `GLFW_PATH`) are supported for custom installations.

