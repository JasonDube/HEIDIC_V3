# Electroscribe IDE

**Electroscribe** is the integrated development environment (IDE) for HEIDIC. It provides a lightweight, Pygame-based editor with integrated compilation, building, and hot-reload support.

## Overview

Electroscribe is designed specifically for HEIDIC development, offering a streamlined workflow from writing code to running games. It features syntax highlighting, project management, and seamless integration with the HEIDIC compiler and EDEN Engine.

## Features

- **Text Editing** - Full-featured code editor with syntax highlighting
- **Project Management** - Create and load HEIDIC projects
- **Integrated Build Pipeline** - Compile, build, and run HEIDIC projects
- **Hot-Reload Support** - Automatic hot-reloading of `@hot` systems
- **C++ View** - Toggle between HEIDIC source and generated C++
- **Output Panels** - Separate panels for compiler output and program terminal
- **Copy to Clipboard** - Easy copying of compiler/terminal output

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
cd ELECTROSCRIBE
pip install -r requirements.txt
```

This will install:
- `pygame` - Required for the GUI
- `watchdog` - Optional, for automatic file watching (hot-reload on save)

**Note:** If `watchdog` is not installed, the editor will still work but automatic file watching will be disabled. You'll need to manually click the hotload button.

## Usage

### Running the Editor

```bash
cd ELECTROSCRIBE
python main.py
```

Or from the project root:
```bash
python ELECTROSCRIBE/main.py
```

### Basic Workflow

1. **Create a New Project**: Click the `+` button in the menu bar
2. **Load a Project**: Click the `[O]` button to browse and load existing projects
3. **Run Project**: Click the `>` (Run) button to compile, build, and run
4. **Hot-Reload**: Edit `@hot` systems and save - they'll automatically reload!

## Project Structure

Projects are stored in `ELECTROSCRIBE/PROJECTS/` directory. Each project is a folder containing:

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

## Source Code

Electroscribe source code is located in:
- `ELECTROSCRIBE/main.py` - Main editor entry point
- `ELECTROSCRIBE/PROJECTS/` - Project files

## Related Tools

- **[EDEN Engine](EDEN_ENGINE.md)** - Runtime game engine
- **[ESE](ESE.md)** - 3D model editor
- **[CONTINUUM Hot-Reload](../CONTINUUM-HOT%20RELOAD%20DOCS/HOT_RELOADING_EXPLAINED.md)** - Hot-reload system

