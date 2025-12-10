# H_SCRIBE - HEIDIC Script Editor

A lightweight Pygame-based script editor for the HEIDIC language with integrated compilation, building, and hot-reload support.

## Installation

### Prerequisites
- Python 3.7 or higher
- Pygame (for GUI)
- Cargo and Rust (for HEIDIC compiler)
- g++ (for C++ compilation)
- Vulkan SDK (for Vulkan projects)
- GLFW (for windowing)

### Install Python Dependencies

```bash
pip install -r requirements.txt
```

This will install:
- `pygame` - Required for the GUI
- `watchdog` - Optional, for automatic file watching (hot-reload on save)

**Note:** If `watchdog` is not installed, the editor will still work but automatic file watching will be disabled. You'll need to manually click the hotload button.

### Running the Editor

```bash
cd H_SCRIBE
python main.py
```

Or from the project root:
```bash
python H_SCRIBE/main.py
```

## Features

- **Text Editing**: Full-featured code editor with syntax highlighting
- **Project Management**: Create and load HEIDIC projects
- **Integrated Build Pipeline**: Compile, build, and run HEIDIC projects
- **Hot-Reload Support**: Automatic hot-reloading of `@hot` systems
- **C++ View**: Toggle between HEIDIC source and generated C++
- **Output Panels**: Separate panels for compiler output and program terminal
- **Copy to Clipboard**: Easy copying of compiler/terminal output

## Usage

1. **Create a New Project**: Click the `+` button in the menu bar
2. **Load a Project**: Click the `[O]` button to browse and load existing projects
3. **Run Project**: Click the `>` (Run) button to compile, build, and run
4. **Hot-Reload**: Edit `@hot` systems and save - they'll automatically reload!

## Project Structure

Projects are stored in `H_SCRIBE/PROJECTS/` directory. Each project is a folder containing:
- `<project_name>.hd` - HEIDIC source file
- `<project_name>.cpp` - Generated C++ code
- `<system_name>_hot.dll.cpp` - Hot-reloadable DLL source (if `@hot` systems exist)
- `<system_name>.dll` - Compiled hot-reloadable DLL
- `<project_name>.exe` - Compiled executable
- `vert_3d.spv`, `frag_3d.spv` - Shader files (auto-copied from examples)

## Hot-Reload Workflow

1. Create a project with `@hot` systems
2. Run the project
3. Edit values in the `@hot` system
4. Save the file (Ctrl+S)
5. The DLL automatically rebuilds and the game reloads it!

## Requirements

- **Python**: 3.7+
- **Pygame**: For GUI (required)
- **watchdog**: For file watching (optional but recommended)
- **Cargo/Rust**: For HEIDIC compiler
- **g++**: For C++ compilation
- **Vulkan SDK**: For Vulkan rendering
- **GLFW**: For windowing

## Troubleshooting

### "No module named 'pygame'"
Install pygame: `pip install pygame`

### "No module named 'watchdog'"
Install watchdog for file watching: `pip install watchdog`
Or continue without it - editor will work but require manual hotload button clicks

### Shaders not found
Shader files are automatically copied when creating new projects. If missing, copy them from `examples/spinning_triangle/` to your project directory.

### DLL locked errors
The game needs time to unload the DLL. The hotload function will retry automatically. Make sure the game is running when you hotload.

