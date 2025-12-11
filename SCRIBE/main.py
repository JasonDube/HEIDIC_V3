"""
SCRIBE - minimal pygame-based HEIDIC script editor scaffold.

Features (current):
- Open a target .hd file (default: examples/hot_reload_test/hot_reload_test.hd)
- Simple text editing (insert/backspace, newlines)
- Save to disk with Ctrl+S
- Trigger HEIDIC compile + build pipeline with F5 (configurable commands)
- Status bar to show messages/compile output

Notes:
- This is intentionally lightweight to get a UI up quickly; not a full editor.
- Pygame is used to avoid extra GUI deps; text rendering uses a monospace font.
- Commands assume you run this from repo root on Windows with cargo/g++ in PATH.
"""

import os
import sys
import subprocess
import re
import pygame
import threading
import queue
import time
from pathlib import Path
try:
    from watchdog.observers import Observer
    from watchdog.events import FileSystemEventHandler
    WATCHDOG_AVAILABLE = True
except ImportError:
    WATCHDOG_AVAILABLE = False
    Observer = None
    FileSystemEventHandler = None

# ---------- Config ----------
# No default file - user must create or load a project
DEFAULT_FILE = None
# Commands can be customized; {file} expands to the current buffer path.
CMD_COMPILE = [
    "cargo",
    "run",
    "--",
    "compile",
    "{file}",
]
# Optional post-compile build step (e.g., gcc). Set to [] to disable.
CMD_BUILD = [
    "g++",
    "-std=c++17",
    "-O3",
    "-I",
    ".",
    "{cpp}",
    "-o",
    "hscribe_build.exe",
]
# Optional run step after build
CMD_RUN = [
    "hscribe_build.exe",
]
SCREEN_WIDTH = 1300
SCREEN_HEIGHT = 700
EDITOR_WIDTH = 900
EDITOR_HEIGHT = 700
SIDE_PANEL_COLOR = (210, 210, 210)
FONT_SIZE = 16
MARGIN = 8
STATUS_HEIGHT = 26
MENU_HEIGHT = 32
BG_COLOR = (18, 18, 18)
FG_COLOR = (230, 230, 230)
STATUS_COLOR = (40, 40, 40)
CURSOR_COLOR = (120, 200, 255)
SCROLL_STEP = 24
MENU_BG_COLOR = (200, 180, 50)  # Yellow menu bar
MENU_TEXT_COLOR = (240, 240, 240)
MENU_BUTTON_SIZE = 32
MENU_BUTTON_SPACING = 4
# Icon buttons: (icon_type, tooltip, action)
# icon_type can be "char" for text or "shape" for drawn shape
MENU_BUTTONS = [
    ("+", "New Project", "new"),
    ("[O]", "Load Project", "load"),
    ("↓", "Save (Ctrl+S)", "save"),
    (">", "Run", "run"),
    ("▶", "Hotload", "hotload"),
    ("◉", "Compile Shaders", "compile_shaders"),
    ("[S]", "Load Shader", "load_shader"),
    ("C++", "Toggle View", "cpp"),  # Cycles HD -> C++ -> SD -> HD
    ("R", "Reload", "reload"),
    ("X", "Quit (Esc)", "quit"),
]
LINE_NUMBER_COLOR = (140, 140, 140)
SCROLLBAR_BG = (60, 60, 60)
SCROLLBAR_FG = (120, 180, 240)
HSCROLLBAR_HEIGHT = 12
LOG_SCROLLBAR_WIDTH = 10
LOG_HSCROLL_HEIGHT = 10
LOG_HEADER_HEIGHT = 32
LOG_HEADER_COLOR = (70, 130, 255)
LOG_COPY_BTN_WIDTH = 28
LOG_COPY_BTN_COLOR = (40, 90, 200)
LOG_COPY_TEXT = "> "
TERMINAL_HEADER_HEIGHT = 32
TERMINAL_HEADER_COLOR = (90, 150, 90)
TERMINAL_COPY_BTN_WIDTH = 28
TERMINAL_COPY_BTN_COLOR = (60, 120, 60)
TERMINAL_MAX_LINES = 500
CPP_BTN_WIDTH = 40
CPP_BTN_HEIGHT = 24
CPP_BTN_COLOR = (100, 150, 255)
CPP_BTN_DISABLED_COLOR = (120, 120, 120)
COLOR_KEYWORD = (180, 140, 255)
COLOR_TYPE = (120, 200, 255)
COLOR_NUMBER = (255, 200, 120)
COLOR_STRING = (140, 220, 140)
COLOR_COMMENT = (140, 170, 220)
COLOR_ANNOTATION = (255, 180, 120)
COLOR_PRINT = (255, 100, 100)


class Editor:
    def __init__(self, path):
        self.path = path
        self.original_hd_path = path if path.endswith(".hd") else path
        self.lines = self._load_file(path)
        # Terminal state (read-only output display)
        self.terminal_lines = []
        self.terminal_vscroll = 0
        self.terminal_hscroll = 0
        self.dragging_terminal_v = False
        self.dragging_terminal_h = False
        self._terminal_drag_offset = 0
        self._terminal_h_drag_offset = 0
        self._init_terminal()
        self.cursor_x = 0
        self.cursor_y = 0
        self.scroll = 0
        self.status = f"Loaded {path}"
        pygame.font.init()
        self.font = pygame.font.SysFont("Consolas", FONT_SIZE)
        self.line_font = pygame.font.SysFont("Consolas", FONT_SIZE)
        self.menu_hitboxes = []
        self.scrollbar_width = 12
        self.line_number_width = 50
        self.dragging_scrollbar = False
        self._drag_offset = 0
        self.hscroll = 0
        self.dragging_hscroll = False
        self._h_drag_offset = 0
        self.log_lines = []
        self.log_vscroll = 0
        self.log_hscroll = 0
        self.dragging_log_v = False
        self.dragging_log_h = False
        self._log_drag_offset = 0
        self._log_h_drag_offset = 0
        self.log_copy_btn_rect = pygame.Rect(MARGIN, (LOG_HEADER_HEIGHT - 22) // 2, LOG_COPY_BTN_WIDTH, 22)
        self.menu_hovered_action = None  # For tooltip display
        self.view_mode = "hd"  # "hd", "cpp", or "shader"
        self.viewing_hd = True  # Deprecated: kept for compatibility, use view_mode instead
        self.current_shader_path = None  # Path to currently loaded shader
        self.original_hd_path = path if path and path.endswith(".hd") else path  # Store original .hd path
        # Initialize view_mode based on path
        if not path or not path.endswith(".hd"):
            self.view_mode = "hd"
            self.viewing_hd = True
        
        # File watching for auto hotload
        self.file_observer = None
        self.auto_hotload_enabled = True  # Enable auto hotload by default
        self.last_hotload_time = 0  # Track last hotload time to avoid rapid-fire reloads
        self._building = False  # Flag to prevent auto-hotload during build
        self._just_built = False  # Flag to prevent auto-hotload immediately after build
        self._setup_file_watcher()
        self.keywords = {
            "fn",
            "let",
            "if",
            "else",
            "while",
            "for",
            "return",
            "struct",
            "enum",
            "system",
            "extern",
            "import",
            "as",
            "break",
            "continue",
            "true",
            "false",
        }
        self.types = {
            "i32",
            "i64",
            "u32",
            "u64",
            "f32",
            "f64",
            "bool",
            "void",
            "String",
        }

    def _text_area_height(self):
        # Height available for text content between menu and status bar, leaving room for horizontal scrollbar and margins
        return EDITOR_HEIGHT - MENU_HEIGHT - STATUS_HEIGHT - HSCROLLBAR_HEIGHT - (MARGIN * 3)

    def _text_area_width(self, has_vscroll):
        # Width available for text content, accounting for line numbers, margins, and vertical scrollbar if present
        return EDITOR_WIDTH - self.line_number_width - (MARGIN * 2) - (self.scrollbar_width if has_vscroll else 0) - MARGIN

    def _load_file(self, path):
        if os.path.exists(path):
            with open(path, "r", encoding="utf-8") as f:
                return f.read().splitlines()
        return [""]
    
    def _init_terminal(self):
        """Initialize the terminal (read-only output display for program output)."""
        self.terminal_lines.append("Terminal ready. Program output will appear here...")
    
    def _add_terminal_line(self, line):
        """Add a line to terminal output, limiting to TERMINAL_MAX_LINES."""
        self.terminal_lines.append(line)
        if len(self.terminal_lines) > TERMINAL_MAX_LINES:
            self.terminal_lines.pop(0)
        # Auto-scroll to bottom
        line_height = self.font.get_linesize()
        max_scroll = max(0, (len(self.terminal_lines) * line_height) - 100)  # Approximate
        self.terminal_vscroll = max_scroll
    
    def _add_terminal_line(self, line):
        """Add a line to terminal output, limiting to TERMINAL_MAX_LINES."""
        self.terminal_lines.append(line)
        if len(self.terminal_lines) > TERMINAL_MAX_LINES:
            self.terminal_lines.pop(0)
        # Auto-scroll to bottom
        line_height = self.font.get_linesize()
        max_scroll = max(0, (len(self.terminal_lines) * line_height) - 100)  # Approximate
        self.terminal_vscroll = max_scroll

    def save(self):
        # Save to current file (HD, C++, or shader)
        if self.view_mode == "shader" and self.current_shader_path:
            save_path = self.current_shader_path
        elif self.view_mode == "cpp" and not self.viewing_hd:
            save_path = self.path  # Save to .cpp file if in C++ view
        else:
            save_path = self.original_hd_path  # Default to .hd file
        
        os.makedirs(os.path.dirname(save_path), exist_ok=True)
        with open(save_path, "w", encoding="utf-8") as f:
            f.write("\n".join(self.lines))
        self.status = f"Saved {os.path.basename(save_path)}"
        
        # Auto hotload if enabled and we have hot systems (only for .hd files)
        # Skip auto-hotload if we're currently building or just finished building
        if self.view_mode == "hd" and self.auto_hotload_enabled and self.has_hot_systems() and not self._building and not self._just_built:
            # Debounce: only hotload if it's been at least 2 seconds since last hotload
            current_time = time.time()
            if current_time - self.last_hotload_time > 2.0:
                self.last_hotload_time = current_time
                # Trigger hotload in a separate thread to avoid blocking
                def delayed_hotload():
                    time.sleep(0.5)  # Small delay to ensure file is fully written
                    self.hotload_dll()
                threading.Thread(target=delayed_hotload, daemon=True).start()
                self.status = f"Saved {save_path} - Auto hotloading..."
    
    def _setup_file_watcher(self):
        """Set up file system watcher for automatic hotload on external file changes."""
        if not WATCHDOG_AVAILABLE:
            return
        
        # Only watch if we have a valid .hd file path
        if not self.original_hd_path or not self.original_hd_path.endswith(".hd"):
            return
        
        try:
            # Stop existing watcher if any
            if self.file_observer:
                self.file_observer.stop()
                self.file_observer.join()
            
            # Create file change handler
            class HotReloadHandler(FileSystemEventHandler):
                def __init__(self, editor):
                    self.editor = editor
                    self.last_modified = 0
                
                def on_modified(self, event):
                    if event.is_directory:
                        return
                    # Ignore DLL files - they get rebuilt during build, don't trigger hotload
                    if event.src_path.endswith(".dll") or event.src_path.endswith(".dll.new"):
                        return
                    # Only handle .hd file changes
                    if event.src_path.endswith(".hd") and os.path.abspath(event.src_path) == os.path.abspath(self.editor.original_hd_path):
                        # Skip if we're building or just finished building
                        if self.editor._building or self.editor._just_built:
                            return
                        # Debounce: avoid multiple triggers from same save
                        current_time = time.time()
                        if current_time - self.last_modified > 1.0:
                            self.last_modified = current_time
                            # Only auto hotload if file wasn't just saved by us (avoid duplicate)
                            if current_time - self.editor.last_hotload_time > 2.0:
                                if self.editor.auto_hotload_enabled and self.editor.has_hot_systems():
                                    self.editor.last_hotload_time = current_time
                                    def delayed_hotload():
                                        time.sleep(0.5)
                                        self.editor.hotload_dll()
                                    threading.Thread(target=delayed_hotload, daemon=True).start()
            
            # Set up observer
            event_handler = HotReloadHandler(self)
            self.file_observer = Observer()
            watch_path = os.path.dirname(os.path.abspath(self.original_hd_path))
            self.file_observer.schedule(event_handler, watch_path, recursive=False)
            self.file_observer.start()
        except Exception as e:
            # Silently fail if file watching doesn't work
            pass
    
    def cleanup(self):
        """Clean up resources (file watchers, etc.)"""
        if self.file_observer:
            try:
                self.file_observer.stop()
                self.file_observer.join()
            except:
                pass
    
    def cpp_file_exists(self):
        """Check if the corresponding .cpp file exists."""
        if not self.original_hd_path.endswith(".hd"):
            return False
        cpp_path = self.original_hd_path.replace(".hd", ".cpp")
        return os.path.exists(cpp_path)
    
    def has_hot_systems(self):
        """Check if the current HEIDIC file has @hot systems, @hot shaders, @hot components, or @hot resources."""
        # Check the original HD file regardless of current view mode
        if not self.original_hd_path or not self.original_hd_path.endswith(".hd"):
            return False
        try:
            with open(self.original_hd_path, "r", encoding="utf-8") as f:
                content = f.read()
                # Check for @hot annotation (systems, shaders, components, or resources)
                return "@hot" in content
        except:
            return False
    
    def has_resources(self):
        """Check if the current HEIDIC file has @hot resource declarations."""
        if not self.original_hd_path or not self.original_hd_path.endswith(".hd"):
            return False
        try:
            with open(self.original_hd_path, "r", encoding="utf-8") as f:
                content = f.read()
                # Only check for @hot resource (not regular resources)
                return "@hot" in content and "resource" in content
        except:
            return False
    
    def find_hot_dll_files(self):
        """Find all hot DLL source files for @hot systems in the current project."""
        if not self.original_hd_path.endswith(".hd"):
            return []
        
        project_dir = os.path.dirname(self.original_hd_path)
        dll_files = []
        
        # Look for *_hot.dll.cpp files in the project directory
        if os.path.exists(project_dir):
            for filename in os.listdir(project_dir):
                if filename.endswith("_hot.dll.cpp"):
                    dll_files.append(os.path.join(project_dir, filename))
        
        return dll_files
    
    def find_hot_shaders(self):
        """Find all @hot shader declarations and return their paths."""
        # Check the original HD file regardless of current view mode
        if not self.original_hd_path or not self.original_hd_path.endswith(".hd"):
            return []
        
        try:
            with open(self.original_hd_path, "r", encoding="utf-8") as f:
                content = f.read()
            
            # Find @hot shader declarations
            # Pattern: @hot shader (vertex|fragment|compute|...) "path"
            import re
            pattern = r'@hot\s+shader\s+\w+\s+"([^"]+)"'
            matches = re.findall(pattern, content)
            
            shader_paths = []
            project_dir = os.path.dirname(self.original_hd_path)
            for match in matches:
                shader_path = match
                # If path is relative, resolve it relative to project directory
                if not os.path.isabs(shader_path):
                    shader_path = os.path.join(project_dir, shader_path)
                # Normalize path
                shader_path = os.path.normpath(shader_path)
                if os.path.exists(shader_path) and shader_path.endswith(('.glsl', '.vert', '.frag', '.comp')):
                    shader_paths.append(shader_path)
            
            return shader_paths
        except Exception as e:
            self.log_lines.append(f"WARNING: Error finding hot shaders: {e}")
            return []
    
    def find_shaders_in_project(self):
        """Find all shader files (.vert, .frag, .glsl, .comp) in the project directory."""
        if not self.original_hd_path.endswith(".hd"):
            return []
        
        project_dir = os.path.dirname(self.original_hd_path)
        shader_files = []
        
        # Look for shader files recursively in project directory
        shader_extensions = ['.vert', '.frag', '.glsl', '.comp', '.geom', '.tesc', '.tese']
        
        for root, dirs, files in os.walk(project_dir):
            for file in files:
                if any(file.endswith(ext) for ext in shader_extensions):
                    shader_path = os.path.join(root, file)
                    # Make path relative to project directory
                    rel_path = os.path.relpath(shader_path, project_dir)
                    shader_files.append((rel_path, shader_path))
        
        return shader_files
    
    def has_shaders(self):
        """Check if the project has any shader files."""
        return len(self.find_shaders_in_project()) > 0
    
    def toggle_view(self):
        """Cycle through view modes: HD -> C++ -> SD -> HD"""
        # Cycle: hd -> cpp -> shader -> hd
        if self.view_mode == "hd":
            # Try to switch to C++
            if self.cpp_file_exists():
                cpp_path = self.original_hd_path.replace(".hd", ".cpp")
                if os.path.exists(cpp_path):
                    self.view_mode = "cpp"
                    self.viewing_hd = False
                    self.path = cpp_path
                    self.lines = self._load_file(cpp_path)
                    self.current_shader_path = None
                    self._reset_editor_state()
                    self.status = f"Viewing C++: {cpp_path}"
                    return
            # If no C++ file, try shader mode
            if self.has_shaders():
                self.view_mode = "shader"
                self.viewing_hd = False
                # Clear compiler output when switching to shader mode
                self.log_lines = []
                self.log_vscroll = 0
                self.log_hscroll = 0
                # Load first shader by default
                shaders = self.find_shaders_in_project()
                if shaders:
                    rel_path, full_path = shaders[0]
                    self.current_shader_path = full_path
                    self.path = full_path
                    self.lines = self._load_file(full_path)
                    self._reset_editor_state()
                    self.log_lines = []  # Clear log again after reset
                    self.status = f"Viewing Shader: {rel_path}"
                    return
        elif self.view_mode == "cpp":
            # Try to switch to shader mode
            if self.has_shaders():
                self.view_mode = "shader"
                self.viewing_hd = False
                # Clear compiler output when switching to shader mode
                self.log_lines = []
                self.log_vscroll = 0
                self.log_hscroll = 0
                shaders = self.find_shaders_in_project()
                if shaders:
                    rel_path, full_path = shaders[0]
                    self.current_shader_path = full_path
                    self.path = full_path
                    self.lines = self._load_file(full_path)
                    self._reset_editor_state()
                    self.log_lines = []  # Clear log again after reset
                    self.status = f"Viewing Shader: {rel_path}"
                    return
            # No shaders, go back to HD
            self.view_mode = "hd"
            self.viewing_hd = True
            self.path = self.original_hd_path
            self.lines = self._load_file(self.original_hd_path)
            self.current_shader_path = None
            self._reset_editor_state()
            self.status = f"Viewing HEIDIC: {self.original_hd_path}"
        elif self.view_mode == "shader":
            # Go back to HD
            self.view_mode = "hd"
            self.viewing_hd = True
            self.path = self.original_hd_path
            self.lines = self._load_file(self.original_hd_path)
            self.current_shader_path = None
            self._reset_editor_state()
            self.status = f"Viewing HEIDIC: {self.original_hd_path}"
    
    def _reset_editor_state(self):
        """Reset editor cursor and scroll state."""
        self.cursor_x = 0
        self.cursor_y = 0
        self.scroll = 0
        self.hscroll = 0
        self.log_lines = []
        self.log_vscroll = 0
        self.log_hscroll = 0
    
    def toggle_cpp_view(self):
        """Legacy method - redirects to toggle_view."""
        self.toggle_view()
    
    def load_shader(self, shader_path):
        """Load a shader file into the editor."""
        if os.path.exists(shader_path):
            self.view_mode = "shader"
            self.viewing_hd = False
            # Clear compiler output when loading shader
            self.log_lines = []
            self.log_vscroll = 0
            self.log_hscroll = 0
            self.current_shader_path = shader_path
            self.path = shader_path
            self.lines = self._load_file(shader_path)
            self._reset_editor_state()
            self.log_lines = []  # Clear log again after reset
            rel_path = os.path.relpath(shader_path, os.path.dirname(self.original_hd_path))
            self.status = f"Viewing Shader: {rel_path}"
            return True
        return False
    
    def compile_shaders_only(self):
        """Compile all shaders in the project without building the full project."""
        if not self.original_hd_path.endswith(".hd"):
            self.status = "No HEIDIC project loaded"
            return
        
        project_dir = os.path.dirname(self.original_hd_path)
        shaders = self.find_shaders_in_project()
        
        if not shaders:
            self.status = "No shaders found in project"
            return
        
        self.status = "Compiling shaders..."
        pygame.display.flip()
        
        # Find glslc
        glslc_found = False
        glslc_path = None
        
        vulkan_sdk = os.environ.get("VULKAN_SDK")
        if vulkan_sdk:
            glslc_path = os.path.join(vulkan_sdk, "bin", "glslc.exe")
            if os.path.exists(glslc_path):
                glslc_found = True
        
        if not glslc_found:
            import shutil
            glslc_path = shutil.which("glslc")
            if glslc_path:
                glslc_found = True
        
        if not glslc_found:
            self.status = "ERROR: glslc not found"
            self.log_lines.append("ERROR: glslc not found. Install Vulkan SDK or ensure glslc is in PATH.")
            return
        
        compiled_count = 0
        for rel_path, shader_path in shaders:
            # Determine output path (.spv) - keep extension to avoid conflicts
            if shader_path.endswith('.glsl'):
                spv_path = shader_path[:-5] + '.spv'
            elif shader_path.endswith('.vert'):
                spv_path = shader_path + '.spv'  # my_shader.vert.spv
            elif shader_path.endswith('.frag'):
                spv_path = shader_path + '.spv'  # my_shader.frag.spv
            elif shader_path.endswith('.comp'):
                spv_path = shader_path + '.spv'  # my_shader.comp.spv
            else:
                spv_path = shader_path + '.spv'
            
            # Determine shader stage
            stage_flag = None
            if shader_path.endswith('.comp'):
                stage_flag = "-fshader-stage=compute"
            elif shader_path.endswith('.vert'):
                stage_flag = "-fshader-stage=vertex"
            elif shader_path.endswith('.frag'):
                stage_flag = "-fshader-stage=fragment"
            elif shader_path.endswith('.geom'):
                stage_flag = "-fshader-stage=geometry"
            
            # Compile shader
            compile_cmd = [glslc_path]
            if stage_flag:
                compile_cmd.append(stage_flag)
            compile_cmd.extend([shader_path, "-o", spv_path])
            
            result = self._run_step(f"CompileShader({os.path.basename(shader_path)})", compile_cmd)
            if result:
                compiled_count += 1
                self.log_lines.append(f"Compiled: {rel_path} -> {os.path.basename(spv_path)}")
            else:
                self.log_lines.append(f"ERROR: Failed to compile {rel_path}")
        
        if compiled_count > 0:
            self.status = f"Compiled {compiled_count} shader(s)"
        else:
            self.status = "Shader compilation failed"

    def handle_key(self, event):
        if event.key == pygame.K_s and pygame.key.get_mods() & pygame.KMOD_CTRL:
            self.save()
            return
        if event.key == pygame.K_BACKSPACE:
            self._backspace()
        elif event.key == pygame.K_RETURN:
            self._newline()
        elif event.key == pygame.K_TAB:
            self._insert("    ")
        elif event.unicode:
            self._insert(event.unicode)

    def _insert(self, text):
        line = self.lines[self.cursor_y]
        self.lines[self.cursor_y] = line[: self.cursor_x] + text + line[self.cursor_x :]
        self.cursor_x += len(text)

    def _backspace(self):
        if self.cursor_x > 0:
            line = self.lines[self.cursor_y]
            self.lines[self.cursor_y] = line[: self.cursor_x - 1] + line[self.cursor_x :]
            self.cursor_x -= 1
        elif self.cursor_y > 0:
            prev = self.lines[self.cursor_y - 1]
            cur = self.lines[self.cursor_y]
            new_x = len(prev)
            self.lines[self.cursor_y - 1] = prev + cur
            del self.lines[self.cursor_y]
            self.cursor_y -= 1
            self.cursor_x = new_x

    def _newline(self):
        line = self.lines[self.cursor_y]
        self.lines[self.cursor_y] = line[: self.cursor_x]
        self.lines.insert(self.cursor_y + 1, line[self.cursor_x :])
        self.cursor_y += 1
        self.cursor_x = 0

    def move_cursor(self, dx, dy):
        self.cursor_y = max(0, min(len(self.lines) - 1, self.cursor_y + dy))
        self.cursor_x = max(0, min(len(self.lines[self.cursor_y]), self.cursor_x + dx))

    def scroll_view(self, dy):
        line_height = self.font.get_linesize()
        usable_height = self._text_area_height()
        total_height = len(self.lines) * line_height
        max_scroll = max(0, total_height - usable_height)
        self.scroll = max(0, min(max_scroll, self.scroll + dy))

    def scroll_view_horizontal(self, dx):
        text_area_width = self._text_area_width(has_vscroll=self.calc_scrollbar() is not None)
        max_line_width = max((self.font.size(line)[0] for line in self.lines), default=0)
        max_scroll = max(0, max_line_width - text_area_width)
        self.hscroll = max(0, min(max_scroll, self.hscroll + dx))

    def scroll_wheel(self, wheel_y):
        # pygame wheel_y: positive means scroll up; invert for typical behavior
        line_height = self.font.get_linesize()
        self.scroll_view(-wheel_y * line_height)

    def calc_scrollbar(self):
        """Return scrollbar geometry or None if not needed."""
        line_height = self.font.get_linesize()
        usable_height = self._text_area_height()
        total_height = len(self.lines) * line_height
        if total_height <= usable_height:
            return None
        bar_height = max(int((usable_height / total_height) * usable_height), 20)
        max_scroll = max(1, total_height - usable_height)
        bar_pos = int((self.scroll / max_scroll) * (usable_height - bar_height))
        scrollbar_x = EDITOR_WIDTH - self.scrollbar_width - MARGIN
        scrollbar_y = MENU_HEIGHT
        return {
            "usable_height": usable_height,
            "total_height": total_height,
            "bar_height": bar_height,
            "bar_pos": bar_pos,
            "scrollbar_x": scrollbar_x,
            "scrollbar_y": scrollbar_y,
            "max_scroll": max_scroll,
        }

    def calc_hscrollbar(self):
        """Return horizontal scrollbar geometry or None if not needed."""
        has_vscroll = self.calc_scrollbar() is not None
        text_area_width = self._text_area_width(has_vscroll)
        max_line_width = max((self.font.size(line)[0] for line in self.lines), default=0)
        if max_line_width <= text_area_width:
            return None
        usable_width = text_area_width
        bar_width = max(int((usable_width / max_line_width) * usable_width), 20)
        max_scroll = max(1, max_line_width - usable_width)
        bar_pos = int((self.hscroll / max_scroll) * (usable_width - bar_width))
        track_x = MARGIN + self.line_number_width
        track_y = MENU_HEIGHT + MARGIN + self._text_area_height() + MARGIN
        return {
            "usable_width": usable_width,
            "bar_width": bar_width,
            "bar_pos": bar_pos,
            "track_x": track_x,
            "track_y": track_y,
            "max_scroll": max_scroll,
        }

    # Log panel helpers
    def calc_log_vscrollbar(self, panel_width, log_height):
        line_height = self.font.get_linesize()
        usable_height = log_height - LOG_HEADER_HEIGHT - (MARGIN * 2) - LOG_HSCROLL_HEIGHT
        total_height = len(self.log_lines) * line_height
        if total_height <= usable_height:
            return None
        bar_height = max(int((usable_height / total_height) * usable_height), 20)
        max_scroll = max(1, total_height - usable_height)
        bar_pos = int((self.log_vscroll / max_scroll) * (usable_height - bar_height))
        scrollbar_x = panel_width - LOG_SCROLLBAR_WIDTH - MARGIN
        scrollbar_y = LOG_HEADER_HEIGHT + MARGIN
        return {
            "usable_height": usable_height,
            "bar_height": bar_height,
            "bar_pos": bar_pos,
            "scrollbar_x": scrollbar_x,
            "scrollbar_y": scrollbar_y,
            "max_scroll": max_scroll,
        }

    def calc_log_hscrollbar(self, panel_width, log_height, log_y):
        text_area_width = panel_width - (MARGIN * 2) - LOG_SCROLLBAR_WIDTH
        max_line_width = max((self.font.size(line)[0] for line in self.log_lines), default=0)
        if max_line_width <= text_area_width:
            return None
        usable_width = text_area_width
        bar_width = max(int((usable_width / max_line_width) * usable_width), 20)
        max_scroll = max(1, max_line_width - usable_width)
        bar_pos = int((self.log_hscroll / max_scroll) * (usable_width - bar_width))
        track_x = MARGIN
        track_y = log_y + log_height - LOG_HSCROLL_HEIGHT - MARGIN
        return {
            "usable_width": usable_width,
            "bar_width": bar_width,
            "bar_pos": bar_pos,
            "track_x": track_x,
            "track_y": track_y,
            "max_scroll": max_scroll,
        }
    
    # Terminal panel helpers
    def calc_terminal_vscrollbar(self, panel_width, terminal_height):
        line_height = self.font.get_linesize()
        usable_height = terminal_height - TERMINAL_HEADER_HEIGHT - (MARGIN * 2) - LOG_HSCROLL_HEIGHT
        total_height = len(self.terminal_lines) * line_height
        if total_height <= usable_height:
            return None
        bar_height = max(int((usable_height / total_height) * usable_height), 20)
        max_scroll = max(1, total_height - usable_height)
        bar_pos = int((self.terminal_vscroll / max_scroll) * (usable_height - bar_height))
        scrollbar_x = panel_width - LOG_SCROLLBAR_WIDTH - MARGIN
        scrollbar_y = TERMINAL_HEADER_HEIGHT + MARGIN
        return {
            "usable_height": usable_height,
            "bar_height": bar_height,
            "bar_pos": bar_pos,
            "scrollbar_x": scrollbar_x,
            "scrollbar_y": scrollbar_y,
            "max_scroll": max_scroll,
        }

    def calc_terminal_hscrollbar(self, panel_width, terminal_height, terminal_y):
        text_area_width = panel_width - (MARGIN * 2) - LOG_SCROLLBAR_WIDTH
        max_line_width = max((self.font.size(line)[0] for line in self.terminal_lines), default=0)
        if max_line_width <= text_area_width:
            return None
        usable_width = text_area_width
        bar_width = max(int((usable_width / max_line_width) * usable_width), 20)
        max_scroll = max(1, max_line_width - usable_width)
        bar_pos = int((self.terminal_hscroll / max_scroll) * (usable_width - bar_width))
        track_x = MARGIN
        track_y = terminal_y + terminal_height - LOG_HSCROLL_HEIGHT - MARGIN
        return {
            "usable_width": usable_width,
            "bar_width": bar_width,
            "bar_pos": bar_pos,
            "track_x": track_x,
            "track_y": track_y,
            "max_scroll": max_scroll,
        }

    def _syntax_tokens_glsl(self, line):
        """Simple syntax highlighter for GLSL shaders."""
        tokens = []
        i = 0
        n = len(line)
        in_string = False
        current = ""
        glsl_keywords = {"layout", "in", "out", "uniform", "vec2", "vec3", "vec4", "mat3", "mat4", 
                         "float", "int", "void", "if", "else", "for", "while", "return", "sampler2D",
                         "gl_Position", "gl_FragColor", "main", "version"}
        while i < n:
            ch = line[i]
            nxt = line[i + 1] if i + 1 < n else ""
            # Comment start
            if not in_string and ch == "/" and nxt == "/":
                if current:
                    tokens.append((current, None))
                    current = ""
                tokens.append((line[i:], COLOR_COMMENT))
                return tokens
            # String literal
            if ch == '"':
                if current:
                    tokens.append((current, None))
                    current = ""
                j = i + 1
                while j < n and line[j] != '"':
                    if line[j] == "\\" and j + 1 < n:
                        j += 2
                    else:
                        j += 1
                # include closing quote if present
                segment = line[i : min(j + 1, n)]
                tokens.append((segment, COLOR_STRING))
                i = j + 1
                continue
            current += ch
            i += 1
        if current:
            tokens.append((current, None))

        # Post-process plain tokens for GLSL keywords/types/numbers
        colored = []
        word_regex = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
        num_regex = re.compile(r"^(\d+(\.\d+)?|0x[0-9A-Fa-f]+)$")
        for text, color in tokens:
            if color is not None:
                colored.append((text, color))
            else:
                # Check for keywords, numbers, etc.
                parts = word_regex.findall(text)
                last_pos = 0
                for word in parts:
                    pos = text.find(word, last_pos)
                    if pos > last_pos:
                        colored.append((text[last_pos:pos], None))
                    if word in glsl_keywords:
                        colored.append((word, COLOR_KEYWORD))
                    elif num_regex.match(word):
                        colored.append((word, COLOR_NUMBER))
                    else:
                        colored.append((word, None))
                    last_pos = pos + len(word)
                if last_pos < len(text):
                    colored.append((text[last_pos:], None))
        return colored
    
    def _syntax_tokens(self, line):
        """Very simple syntax highlighter for HEIDIC-like syntax."""
        tokens = []
        i = 0
        n = len(line)
        in_string = False
        current = ""
        while i < n:
            ch = line[i]
            nxt = line[i + 1] if i + 1 < n else ""
            # Comment start
            if not in_string and ch == "/" and nxt == "/":
                if current:
                    tokens.append((current, None))
                    current = ""
                tokens.append((line[i:], COLOR_COMMENT))
                return tokens
            # String literal
            if ch == '"':
                if current:
                    tokens.append((current, None))
                    current = ""
                j = i + 1
                while j < n and line[j] != '"':
                    if line[j] == "\\" and j + 1 < n:
                        j += 2
                    else:
                        j += 1
                # include closing quote if present
                segment = line[i : min(j + 1, n)]
                tokens.append((segment, COLOR_STRING))
                i = j + 1
                continue
            current += ch
            i += 1
        if current:
            tokens.append((current, None))

        # Post-process plain tokens for keywords/types/numbers/annotations
        colored = []
        word_regex = re.compile(r"@?[A-Za-z_][A-Za-z0-9_]*")
        num_regex = re.compile(r"^(\d+(\.\d+)?|0x[0-9A-Fa-f]+)$")
        for text, color in tokens:
            if color:
                colored.append((text, color))
                continue
            pos = 0
            while pos < len(text):
                m = word_regex.search(text, pos)
                if not m:
                    colored.append((text[pos:], None))
                    break
                if m.start() > pos:
                    colored.append((text[pos : m.start()], None))
                word = m.group(0)
                if word in self.keywords:
                    colored.append((word, COLOR_KEYWORD))
                elif word in self.types:
                    colored.append((word, COLOR_TYPE))
                elif word == "print":
                    colored.append((word, COLOR_PRINT))
                elif word.startswith("@"):
                    colored.append((word, COLOR_ANNOTATION))
                elif num_regex.match(word):
                    colored.append((word, COLOR_NUMBER))
                else:
                    colored.append((word, None))
                pos = m.end()
        return colored

    def _build_menu_hitboxes(self):
        """Compute clickable icon button regions on the editor surface."""
        self.menu_hitboxes = []
        x = MARGIN
        for icon_char, tooltip, action in MENU_BUTTONS:
            rect = pygame.Rect(x, (MENU_HEIGHT - MENU_BUTTON_SIZE) // 2, MENU_BUTTON_SIZE, MENU_BUTTON_SIZE)
            self.menu_hitboxes.append((rect, icon_char, tooltip, action))
            x += MENU_BUTTON_SIZE + MENU_BUTTON_SPACING

    def draw(self, screen):
        screen.fill(SIDE_PANEL_COLOR)
        editor_surface = pygame.Surface((EDITOR_WIDTH, EDITOR_HEIGHT))
        editor_surface.fill(BG_COLOR)

        # Menu bar
        pygame.draw.rect(
            editor_surface,
            MENU_BG_COLOR,
            pygame.Rect(0, 0, EDITOR_WIDTH, MENU_HEIGHT),
        )
        self._build_menu_hitboxes()
        
        # Draw icon buttons
        icon_font = pygame.font.SysFont("Consolas", 18)  # Slightly larger for icons
        cpp_exists = self.cpp_file_exists()
        has_project = self.original_hd_path and self.original_hd_path.endswith(".hd") and os.path.exists(self.original_hd_path)
        has_shaders = self.has_shaders() if has_project else False
        
        for rect, icon_char, tooltip, action in self.menu_hitboxes:
            # Check if button should be disabled
            is_disabled = False
            if action == "cpp":
                # Disable toggle button if no project loaded OR (no C++ file AND no shaders)
                is_disabled = not has_project or (not cpp_exists and not has_shaders)
            elif action == "hotload" and not self.has_hot_systems():
                is_disabled = True
            elif action == "load_shader":
                # Disable if no project or no shaders
                is_disabled = not (has_project and has_shaders)
            elif action == "compile_shaders":
                # Disable if no project or no shaders
                is_disabled = not (has_project and has_shaders)
            
            # Button background (with hover effect)
            if is_disabled:
                pygame.draw.rect(editor_surface, (40, 40, 40), rect)
                pygame.draw.rect(editor_surface, (60, 60, 60), rect, 1)
            elif self.menu_hovered_action == action:
                pygame.draw.rect(editor_surface, (60, 60, 60), rect)
                pygame.draw.rect(editor_surface, (100, 150, 255), rect, 2)
            else:
                pygame.draw.rect(editor_surface, (45, 45, 45), rect)
                pygame.draw.rect(editor_surface, (70, 70, 70), rect, 1)
            
            # Draw icon based on action
            center_x = rect.x + rect.width // 2
            center_y = rect.y + rect.height // 2
            icon_color = (150, 150, 150) if is_disabled else MENU_TEXT_COLOR
            
            if action == "new":
                # Plus sign
                pygame.draw.line(editor_surface, icon_color, (center_x - 8, center_y), (center_x + 8, center_y), 2)
                pygame.draw.line(editor_surface, icon_color, (center_x, center_y - 8), (center_x, center_y + 8), 2)
            elif action == "load":
                # Folder icon (simple)
                folder_rect = pygame.Rect(center_x - 10, center_y - 6, 12, 8)
                pygame.draw.rect(editor_surface, icon_color, folder_rect, 2)
                pygame.draw.line(editor_surface, icon_color, (center_x - 8, center_y - 6), (center_x - 8, center_y - 2), 2)
            elif action == "save":
                # Disk icon (circle with line)
                pygame.draw.circle(editor_surface, icon_color, (center_x, center_y - 2), 6, 2)
                pygame.draw.line(editor_surface, icon_color, (center_x, center_y + 4), (center_x, center_y + 8), 2)
            elif action == "run":
                # Play triangle
                points = [(center_x - 6, center_y - 6), (center_x - 6, center_y + 6), (center_x + 6, center_y)]
                pygame.draw.polygon(editor_surface, icon_color, points)
            elif action == "hotload":
                # Red arrow (right-pointing triangle in red)
                arrow_color = (255, 80, 80) if not is_disabled else (150, 150, 150)
                points = [(center_x - 6, center_y - 6), (center_x - 6, center_y + 6), (center_x + 6, center_y)]
                pygame.draw.polygon(editor_surface, arrow_color, points)
            elif action == "compile_shaders":
                # Blue arrow for shader compilation
                arrow_color = (80, 120, 255) if not is_disabled else (150, 150, 150)
                points = [(center_x - 6, center_y - 6), (center_x - 6, center_y + 6), (center_x + 6, center_y)]
                pygame.draw.polygon(editor_surface, arrow_color, points)
            elif action == "load_shader":
                # Shader icon (simple "S")
                shader_surf = icon_font.render("S", True, icon_color)
                shader_x = center_x - shader_surf.get_width() // 2
                shader_y = center_y - shader_surf.get_height() // 2
                editor_surface.blit(shader_surf, (shader_x, shader_y))
            elif action == "cpp":
                # Show current view: "HD", "C++", or "SD"
                if self.view_mode == "hd":
                    btn_text = "HD"
                elif self.view_mode == "cpp":
                    btn_text = "C++"
                elif self.view_mode == "shader":
                    btn_text = "SD"
                else:
                    btn_text = "HD"
                cpp_surf = icon_font.render(btn_text, True, icon_color)
                cpp_x = center_x - cpp_surf.get_width() // 2
                cpp_y = center_y - cpp_surf.get_height() // 2
                editor_surface.blit(cpp_surf, (cpp_x, cpp_y))
            elif action == "reload":
                # Circular arrow (simplified - just a circle with arrow)
                pygame.draw.circle(editor_surface, icon_color, (center_x, center_y), 8, 2)
                pygame.draw.line(editor_surface, icon_color, (center_x + 6, center_y - 4), (center_x + 8, center_y - 6), 2)
            elif action == "quit":
                # X mark
                pygame.draw.line(editor_surface, icon_color, (center_x - 6, center_y - 6), (center_x + 6, center_y + 6), 2)
                pygame.draw.line(editor_surface, icon_color, (center_x + 6, center_y - 6), (center_x - 6, center_y + 6), 2)
        
        # Tooltip (if hovering)
        if self.menu_hovered_action:
            for rect, icon_char, tooltip, action in self.menu_hitboxes:
                if action == self.menu_hovered_action:
                    # Dynamic tooltip for view toggle button
                    if action == "cpp":
                        if self.view_mode == "hd":
                            tooltip_text = "View C++"
                        elif self.view_mode == "cpp":
                            tooltip_text = "View Shader"
                        elif self.view_mode == "shader":
                            tooltip_text = "View HEIDIC"
                        else:
                            tooltip_text = "Toggle View"
                    else:
                        tooltip_text = tooltip
                    tooltip_surf = self.font.render(tooltip_text, True, (240, 240, 240))
                    tooltip_bg = pygame.Surface((tooltip_surf.get_width() + 8, tooltip_surf.get_height() + 4))
                    tooltip_bg.fill((50, 50, 50))
                    tooltip_bg.set_alpha(240)
                    tooltip_x = rect.x
                    tooltip_y = rect.bottom + 4
                    if tooltip_y + tooltip_bg.get_height() > MENU_HEIGHT + 30:
                        tooltip_y = rect.top - tooltip_bg.get_height() - 4
                    editor_surface.blit(tooltip_bg, (tooltip_x, tooltip_y))
                    editor_surface.blit(tooltip_surf, (tooltip_x + 4, tooltip_y + 2))
                    break
        
        text_area_height = self._text_area_height()
        text_area_start_y = MENU_HEIGHT + MARGIN
        line_height = self.font.get_linesize()
        first_line = self.scroll // line_height
        max_lines = text_area_height // line_height
        has_vscroll = self.calc_scrollbar() is not None
        text_area_width = self._text_area_width(has_vscroll)

        # Draw text lines
        for i in range(max_lines + 1):
            idx = first_line + i
            if idx >= len(self.lines):
                break
            # Line number
            ln_text = str(idx + 1)
            ln_surf = self.line_font.render(ln_text, True, (140, 140, 140))
            editor_surface.blit(
                ln_surf,
                (
                    MARGIN,
                    text_area_start_y + i * line_height,
                ),
            )
            # Syntax-colored text (only for .hd files)
            if self.view_mode == "shader":
                # GLSL syntax highlighting (simple)
                tokens = self._syntax_tokens_glsl(self.lines[idx])
            elif self.view_mode == "hd":
                tokens = self._syntax_tokens(self.lines[idx])
            else:
                tokens = [(self.lines[idx], None)]  # No syntax coloring for C++
            x_cursor = MARGIN + self.line_number_width - self.hscroll
            y = text_area_start_y + i * line_height
            for segment, color in tokens:
                seg_color = color if color else FG_COLOR
                seg_surf = self.font.render(segment, True, seg_color)
                # Only draw if segment is visible horizontally
                if x_cursor + seg_surf.get_width() >= MARGIN + self.line_number_width - self.hscroll:
                    editor_surface.blit(seg_surf, (x_cursor, y))
                x_cursor += seg_surf.get_width()

        # Draw cursor
        cursor_px_y = self.cursor_y * line_height - self.scroll + text_area_start_y
        if text_area_start_y <= cursor_px_y < text_area_start_y + text_area_height:
            cursor_px_x = self.font.size(self.lines[self.cursor_y][: self.cursor_x])[0] + MARGIN + self.line_number_width - self.hscroll
            pygame.draw.line(
                editor_surface,
                CURSOR_COLOR,
                (cursor_px_x, cursor_px_y),
                (cursor_px_x, cursor_px_y + line_height - 2),
                2,
            )

        # Status bar
        pygame.draw.rect(
            editor_surface,
            STATUS_COLOR,
            pygame.Rect(0, EDITOR_HEIGHT - STATUS_HEIGHT, EDITOR_WIDTH, STATUS_HEIGHT),
        )
        status_text = self.font.render(self.status, True, FG_COLOR)
        editor_surface.blit(status_text, (MARGIN, EDITOR_HEIGHT - STATUS_HEIGHT + 4))

        # Scrollbar
        sb = self.calc_scrollbar()
        if sb:
            pygame.draw.rect(
                editor_surface,
                SCROLLBAR_BG,
                pygame.Rect(sb["scrollbar_x"], sb["scrollbar_y"], self.scrollbar_width, sb["usable_height"]),
            )
            pygame.draw.rect(
                editor_surface,
                SCROLLBAR_FG,
                pygame.Rect(sb["scrollbar_x"], sb["scrollbar_y"] + sb["bar_pos"], self.scrollbar_width, sb["bar_height"]),
            )

        # Horizontal scrollbar
        hsb = self.calc_hscrollbar()
        if hsb:
            track_rect = pygame.Rect(
                hsb["track_x"],
                hsb["track_y"],
                hsb["usable_width"],
                HSCROLLBAR_HEIGHT,
            )
            pygame.draw.rect(editor_surface, SCROLLBAR_BG, track_rect)
            thumb_rect = pygame.Rect(
                hsb["track_x"] + hsb["bar_pos"],
                hsb["track_y"],
                hsb["bar_width"],
                HSCROLLBAR_HEIGHT,
            )
            pygame.draw.rect(editor_surface, SCROLLBAR_FG, thumb_rect)

        # Right-justify editor; left side is side panel
        offset_x = SCREEN_WIDTH - EDITOR_WIDTH
        screen.blit(editor_surface, (offset_x, 0))

        # Left panel: split into log (top) and terminal (bottom) halves
        panel_width = SCREEN_WIDTH - EDITOR_WIDTH
        panel_surface = pygame.Surface((panel_width, SCREEN_HEIGHT))
        panel_surface.fill(SIDE_PANEL_COLOR)
        
        # Split panel in half
        log_height = SCREEN_HEIGHT // 2
        terminal_height = SCREEN_HEIGHT - log_height
        terminal_y = log_height
        
        # Terminal is read-only, no processing needed
        
        # === LOG SECTION (top half) ===
        pygame.draw.rect(panel_surface, LOG_HEADER_COLOR, pygame.Rect(0, 0, panel_width, LOG_HEADER_HEIGHT))
        # Copy button
        pygame.draw.rect(panel_surface, LOG_COPY_BTN_COLOR, self.log_copy_btn_rect)
        btn_text = self.font.render(">", True, (230, 230, 230))
        btn_text_rect = btn_text.get_rect(center=self.log_copy_btn_rect.center)
        panel_surface.blit(btn_text, btn_text_rect)
        line_height = self.font.get_linesize()
        v_sb = self.calc_log_vscrollbar(panel_width, log_height)
        h_sb = self.calc_log_hscrollbar(panel_width, log_height, 0)
        usable_height = log_height - LOG_HEADER_HEIGHT - (MARGIN * 2) - (LOG_HSCROLL_HEIGHT if h_sb else 0)
        usable_width = panel_width - (MARGIN * 2) - (LOG_SCROLLBAR_WIDTH if v_sb else 0)

        first_line = self.log_vscroll // line_height
        max_lines = usable_height // line_height
        # Calculate max Y position to avoid drawing past horizontal scrollbar
        log_max_y = log_height - (LOG_HSCROLL_HEIGHT if h_sb else 0) - MARGIN
        for i in range(max_lines + 1):
            idx = first_line + i
            if idx >= len(self.log_lines):
                break
            text = self.log_lines[idx]
            x = MARGIN - self.log_hscroll
            y = LOG_HEADER_HEIGHT + MARGIN + i * line_height
            # Don't draw if past the horizontal scrollbar
            if y + line_height > log_max_y:
                break
            # Render bold black text (render twice with slight offset for bold effect)
            text_surf = self.font.render(text, True, (0, 0, 0))
            panel_surface.blit(text_surf, (x, y))
            # Render again with slight offset for bold effect
            panel_surface.blit(text_surf, (x + 1, y))

        # Draw log scrollbars
        if v_sb:
            pygame.draw.rect(
                panel_surface,
                SCROLLBAR_BG,
                pygame.Rect(v_sb["scrollbar_x"], v_sb["scrollbar_y"], LOG_SCROLLBAR_WIDTH, v_sb["usable_height"]),
            )
            pygame.draw.rect(
                panel_surface,
                SCROLLBAR_FG,
                pygame.Rect(v_sb["scrollbar_x"], v_sb["scrollbar_y"] + v_sb["bar_pos"], LOG_SCROLLBAR_WIDTH, v_sb["bar_height"]),
            )
        if h_sb:
            track_rect = pygame.Rect(
                h_sb["track_x"],
                h_sb["track_y"],
                h_sb["usable_width"],
                LOG_HSCROLL_HEIGHT,
            )
            pygame.draw.rect(panel_surface, SCROLLBAR_BG, track_rect)
            thumb_rect = pygame.Rect(
                h_sb["track_x"] + h_sb["bar_pos"],
                h_sb["track_y"],
                h_sb["bar_width"],
                LOG_HSCROLL_HEIGHT,
            )
            pygame.draw.rect(panel_surface, SCROLLBAR_FG, thumb_rect)

        # === TERMINAL SECTION (bottom half) ===
        terminal_header_y = terminal_y
        pygame.draw.rect(panel_surface, TERMINAL_HEADER_COLOR, pygame.Rect(0, terminal_header_y, panel_width, TERMINAL_HEADER_HEIGHT))
        # Terminal copy button
        terminal_copy_btn = pygame.Rect(MARGIN, terminal_header_y + (TERMINAL_HEADER_HEIGHT - 22) // 2, TERMINAL_COPY_BTN_WIDTH, 22)
        pygame.draw.rect(panel_surface, TERMINAL_COPY_BTN_COLOR, terminal_copy_btn)
        btn_text = self.font.render(">", True, (230, 230, 230))
        btn_text_rect = btn_text.get_rect(center=terminal_copy_btn.center)
        panel_surface.blit(btn_text, btn_text_rect)
        
        v_sb_term = self.calc_terminal_vscrollbar(panel_width, terminal_height)
        h_sb_term = self.calc_terminal_hscrollbar(panel_width, terminal_height, terminal_y)
        term_usable_height = terminal_height - TERMINAL_HEADER_HEIGHT - (MARGIN * 2) - (LOG_HSCROLL_HEIGHT if h_sb_term else 0)
        term_usable_width = panel_width - (MARGIN * 2) - (LOG_SCROLLBAR_WIDTH if v_sb_term else 0)

        first_line_term = self.terminal_vscroll // line_height
        max_lines_term = term_usable_height // line_height
        # Calculate max Y position to avoid drawing past horizontal scrollbar
        max_y = terminal_header_y + terminal_height - (LOG_HSCROLL_HEIGHT if h_sb_term else 0) - MARGIN
        for i in range(max_lines_term + 1):
            idx = first_line_term + i
            if idx >= len(self.terminal_lines):
                break
            text = self.terminal_lines[idx]
            x = MARGIN - self.terminal_hscroll
            y = terminal_header_y + TERMINAL_HEADER_HEIGHT + MARGIN + i * line_height
            # Don't draw if past the horizontal scrollbar
            if y + line_height > max_y:
                break
            # Render bold black text (render twice with slight offset for bold effect)
            text_surf = self.font.render(text, True, (0, 0, 0))
            panel_surface.blit(text_surf, (x, y))
            # Render again with slight offset for bold effect
            panel_surface.blit(text_surf, (x + 1, y))

        # Draw terminal scrollbars
        if v_sb_term:
            scrollbar_y_pos = terminal_header_y + TERMINAL_HEADER_HEIGHT + MARGIN
            pygame.draw.rect(
                panel_surface,
                SCROLLBAR_BG,
                pygame.Rect(v_sb_term["scrollbar_x"], scrollbar_y_pos, LOG_SCROLLBAR_WIDTH, v_sb_term["usable_height"]),
            )
            pygame.draw.rect(
                panel_surface,
                SCROLLBAR_FG,
                pygame.Rect(v_sb_term["scrollbar_x"], scrollbar_y_pos + v_sb_term["bar_pos"], LOG_SCROLLBAR_WIDTH, v_sb_term["bar_height"]),
            )
        if h_sb_term:
            track_rect = pygame.Rect(
                h_sb_term["track_x"],
                h_sb_term["track_y"],
                h_sb_term["usable_width"],
                LOG_HSCROLL_HEIGHT,
            )
            pygame.draw.rect(panel_surface, SCROLLBAR_BG, track_rect)
            thumb_rect = pygame.Rect(
                h_sb_term["track_x"] + h_sb_term["bar_pos"],
                h_sb_term["track_y"],
                h_sb_term["bar_width"],
                LOG_HSCROLL_HEIGHT,
            )
            pygame.draw.rect(panel_surface, SCROLLBAR_FG, thumb_rect)

        screen.blit(panel_surface, (0, 0))

    def run_commands(self):
        """Compile -> build -> run pipeline with status updates."""
        # Set building flag to prevent auto-hotload during build
        self._building = True
        try:
            self.save()
            self.status = "Compiling..."
            pygame.display.flip()
            hd_path = os.path.abspath(self.original_hd_path)
            cpp_path = hd_path.replace(".hd", ".cpp")

            def fmt_cmd(cmd):
                return [p.format(file=hd_path, cpp=cpp_path) for p in cmd]

            # Transpile timing
            transpile_start = time.time()
            result = self._run_step("Compile", fmt_cmd(CMD_COMPILE))
            transpile_time = (time.time() - transpile_start) * 1000  # Convert to ms
            
            # Initialize variables to avoid UnboundLocalError
            shader_compile_time = 0.0
            build_time = 0.0
            if result:
                # Build timing
                build_start = time.time()
                shader_compile_time = self._do_build(hd_path, cpp_path)
                build_time = time.time() - build_start
                result = True  # Assume success for now (errors are in log)
            
            # Display timing info in terminal
            self._add_terminal_line("")
            self._add_terminal_line("=== Build Summary ===")
            self._add_terminal_line(f"Transpile time: {transpile_time:.2f} ms")
            if shader_compile_time > 0:
                self._add_terminal_line(f"Shader compilation time: {shader_compile_time:.2f} seconds")
            self._add_terminal_line(f"Compile + Link time: {build_time:.2f} seconds")
            self._add_terminal_line("")
            
            # Check if executable exists before trying to run
            script_dir = os.path.dirname(os.path.abspath(__file__))
            project_root = os.path.abspath(os.path.join(script_dir, ".."))
            project_dir = os.path.dirname(cpp_path)
            exe_name = os.path.splitext(os.path.basename(hd_path))[0] + ".exe"
            exe_path = os.path.join(project_dir, exe_name)
            if os.path.exists(exe_path) and CMD_RUN:
                # Run program and capture output to terminal in real-time
                self.status = "Run launched"
                self.log_lines.append(f"Run: launched {exe_name}")
                def run_with_output():
                    try:
                        # Use unbuffered output and set environment for immediate flushing
                        env = os.environ.copy()
                        env['PYTHONUNBUFFERED'] = '1'  # For Python programs
                        # Update CMD_RUN to use the correct executable path
                        run_cmd = [exe_path]
                        proc = subprocess.Popen(
                            run_cmd,
                            cwd=project_dir,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT,
                            text=True,
                            bufsize=0,  # Unbuffered
                            shell=False,
                            env=env,
                        )
                        # Read output line by line in real-time
                        import sys
                        for line in proc.stdout:
                            if line:
                                line = line.rstrip("\n\r")
                                if line:  # Only add non-empty lines
                                    self._add_terminal_line(line)
                        proc.wait()
                    except Exception as e:
                        self._add_terminal_line(f"Error running program: {e}")
                # threading is already imported at module level (line 22)
                thread = threading.Thread(target=run_with_output, daemon=True)
                thread.start()
            elif CMD_RUN:
                self.log_lines.append("ERROR: Executable not found. Build may have failed.")
                self.status = "Build failed - cannot run"
        finally:
            # Reset building flag after build completes
            self._building = False
            # Set just_built flag to prevent immediate auto-hotload
            self._just_built = True
            # Clear just_built flag after 5 seconds (enough time for file watcher to stabilize)
            def clear_just_built():
                time.sleep(5.0)
                self._just_built = False
            # threading is already imported at module level (line 22)
            threading.Thread(target=clear_just_built, daemon=True).start()
    
    def _compile_shaders_for_build(self, project_dir):
        """Compile shaders in the project and output to compiler log.
        Returns: (success: bool, compilation_time: float in seconds)
        
        Compiles ALL .vert, .frag, .comp, .geom shaders in the project's shaders/ folder,
        not just @hot shaders. This ensures standard shaders (like quad.vert/frag) are
        automatically compiled.
        """
        # Find all shaders in the project (both @hot and standard shaders)
        project_shaders_dir = os.path.join(project_dir, "shaders")
        shaders_to_compile = []
        
        # Add @hot shaders from HEIDIC code
        hot_shaders = self.find_hot_shaders()
        for shader_path in hot_shaders:
            if shader_path not in shaders_to_compile:
                shaders_to_compile.append(shader_path)
        
        # Also compile all standard shader files in shaders/ folder
        if os.path.exists(project_shaders_dir):
            for file in os.listdir(project_shaders_dir):
                if file.endswith(('.vert', '.frag', '.comp', '.geom', '.glsl')):
                    shader_path = os.path.join(project_shaders_dir, file)
                    if shader_path not in shaders_to_compile:
                        shaders_to_compile.append(shader_path)
        
        if not shaders_to_compile:
            return True, 0.0  # No shaders to compile, return 0 time
        
        shader_compile_start = time.time()
        self.log_lines.append("Compiling shaders...")
        
        # Find glslc
        glslc_found = False
        glslc_path = None
        
        vulkan_sdk = os.environ.get("VULKAN_SDK")
        if vulkan_sdk:
            glslc_path = os.path.join(vulkan_sdk, "bin", "glslc.exe")
            if os.path.exists(glslc_path):
                glslc_found = True
        
        if not glslc_found:
            import shutil
            glslc_path = shutil.which("glslc")
            if glslc_path:
                glslc_found = True
        
        if not glslc_found:
            self.log_lines.append("ERROR: glslc not found. Install Vulkan SDK or ensure glslc is in PATH.")
            shader_compile_time = time.time() - shader_compile_start
            return False, shader_compile_time
        
        compiled_count = 0
        failed_count = 0
        
        for glsl_path in shaders_to_compile:
            # Determine output path (.spv) - keep extension to avoid conflicts
            if glsl_path.endswith('.glsl'):
                spv_path = glsl_path[:-5] + '.spv'
            elif glsl_path.endswith('.vert'):
                spv_path = glsl_path + '.spv'  # my_shader.vert.spv
            elif glsl_path.endswith('.frag'):
                spv_path = glsl_path + '.spv'  # my_shader.frag.spv
            elif glsl_path.endswith('.comp'):
                spv_path = glsl_path + '.spv'  # my_shader.comp.spv
            else:
                spv_path = glsl_path + '.spv'
            
            # Determine shader stage for glslc
            stage_flag = None
            if glsl_path.endswith('.comp'):
                stage_flag = "-fshader-stage=compute"
            elif glsl_path.endswith('.vert'):
                stage_flag = "-fshader-stage=vertex"
            elif glsl_path.endswith('.frag'):
                stage_flag = "-fshader-stage=fragment"
            elif glsl_path.endswith('.geom'):
                stage_flag = "-fshader-stage=geometry"
            
            # Compile shader
            compile_cmd = [glslc_path]
            if stage_flag:
                compile_cmd.append(stage_flag)
            compile_cmd.extend([glsl_path, "-o", spv_path])
            
            rel_path = os.path.relpath(glsl_path, project_dir)
            result = self._run_step(f"Shader({os.path.basename(glsl_path)})", compile_cmd)
            if result:
                compiled_count += 1
                self.log_lines.append(f"[OK] Compiled: {rel_path} -> {os.path.basename(spv_path)}")
            else:
                failed_count += 1
                self.log_lines.append(f"[FAIL] Failed: {rel_path}")
        
        shader_compile_time = time.time() - shader_compile_start
        
        if compiled_count > 0:
            self.log_lines.append(f"Shader compilation: {compiled_count} succeeded, {failed_count} failed")
        elif failed_count > 0:
            self.log_lines.append(f"Shader compilation failed: {failed_count} shader(s) failed")
        
        return failed_count == 0, shader_compile_time
    
    def _do_build(self, hd_path, cpp_path):
        """Build the C++ code with all necessary libraries and includes."""
        # Get project root (two levels up from SCRIBE)
        script_dir = os.path.dirname(os.path.abspath(__file__))
        project_root = os.path.abspath(os.path.join(script_dir, ".."))
        project_dir = os.path.dirname(cpp_path)  # Project directory (where .hd file is)
        vulkan_helpers_path = os.path.join(project_root, "vulkan", "eden_vulkan_helpers.cpp")
        
        # Compile shaders first (if we have hot shaders)
        shader_compile_success, shader_compile_time = self._compile_shaders_for_build(project_dir)
        
        # Ensure shader files exist in project directory
        shader_source_dir = os.path.join(project_root, "examples", "spinning_triangle")
        shaders_to_copy = ["vert_3d.spv", "frag_3d.spv"]
        for shader_file in shaders_to_copy:
            dest_shader = os.path.join(project_dir, shader_file)
            if not os.path.exists(dest_shader):
                source_shader = os.path.join(shader_source_dir, shader_file)
                if os.path.exists(source_shader):
                    try:
                        import shutil
                        shutil.copy2(source_shader, dest_shader)
                        self.log_lines.append(f"Copied shader: {shader_file}")
                    except Exception as e:
                        self.log_lines.append(f"WARNING: Failed to copy {shader_file}: {e}")
                else:
                    self.log_lines.append(f"WARNING: Shader source not found: {source_shader}")
        
        # Start with base command
        build_cmd = ["g++", "-std=c++17", "-O3"]
        
        # Add include paths
        build_cmd.extend(["-I", project_root])  # For stdlib/vulkan.h
        
        # Add Vulkan SDK include if available
        vulkan_sdk = os.environ.get("VULKAN_SDK")
        if vulkan_sdk:
            inc = os.path.join(vulkan_sdk, "Include")
            build_cmd.extend(["-I", inc])
        else:
            self.log_lines.append("WARNING: VULKAN_SDK not set; Vulkan headers may be missing")
        
        # Add GLFW include path (try common locations or env var)
        glfw_path = os.environ.get("GLFW_PATH")
        if not glfw_path:
            # Try common locations
            for path in ["C:\\glfw-3.4", "C:\\glfw"]:
                if os.path.exists(path):
                    glfw_path = path
                    break
        
        if glfw_path:
            glfw_include = os.path.join(glfw_path, "include")
            if os.path.exists(glfw_include):
                build_cmd.extend(["-I", glfw_include])
            else:
                build_cmd.extend(["-I", glfw_path])
        
        # Add source files
        build_cmd.append(cpp_path)
        if os.path.exists(vulkan_helpers_path):
            build_cmd.append(vulkan_helpers_path)
        else:
            self.log_lines.append(f"WARNING: Vulkan helpers not found at {vulkan_helpers_path}")
        
        # Add output - build to project directory
        exe_name = os.path.splitext(os.path.basename(hd_path))[0] + ".exe"
        exe_path = os.path.join(project_dir, exe_name)
        
        # DIAGNOSTIC: Force delete old exe to prevent stale binary issues
        if os.path.exists(exe_path):
            try:
                os.remove(exe_path)
                self.log_lines.append(f"[DEBUG] Deleted old {exe_name} to force rebuild")
            except Exception as e:
                self.log_lines.append(f"[DEBUG] Could not delete {exe_name}: {e}")
        
        build_cmd.extend(["-o", exe_path])
        
        # Add library paths
        if vulkan_sdk:
            build_cmd.extend(["-L", os.path.join(vulkan_sdk, "Lib")])
        
        if glfw_path:
            # Try common GLFW library locations
            for lib_path in [os.path.join(glfw_path, "build", "src"), 
                           os.path.join(glfw_path, "lib"),
                           os.path.join(glfw_path, "lib-mingw-w64")]:
                if os.path.exists(lib_path):
                    build_cmd.extend(["-L", lib_path])
                    break
        
        # Add libraries to link
        build_cmd.extend(["-lvulkan-1", "-lglfw3", "-lgdi32"])
        
        self._run_step("Build", build_cmd)
        
        # DIAGNOSTIC: Verify build succeeded by checking timestamps
        if os.path.exists(exe_path) and os.path.exists(cpp_path):
            exe_time = os.path.getmtime(exe_path)
            cpp_time = os.path.getmtime(cpp_path)
            if exe_time > cpp_time:
                self.log_lines.append(f"[DEBUG] Build OK: {exe_name} is newer than source")
            else:
                self.log_lines.append(f"[DEBUG] WARNING: {exe_name} may be stale (older than source)")
        
        # Also build hot-reloadable DLLs if they exist
        project_dir = os.path.dirname(cpp_path)
        dll_files = self.find_hot_dll_files()
        if dll_files:
            self.log_lines.append(f"Building {len(dll_files)} hot-reloadable DLL(s)...")
            for dll_cpp_path in dll_files:
                # Extract system name from filename (e.g., "rotation_hot.dll.cpp" -> "rotation")
                dll_cpp_name = os.path.basename(dll_cpp_path)
                system_name = dll_cpp_name.replace("_hot.dll.cpp", "")
                dll_name = f"{system_name}.dll"
                dll_path = os.path.join(project_dir, dll_name)
                
                # Compile DLL
                dll_build_cmd = [
                    "g++",
                    "-std=c++17",
                    "-shared",
                    "-fPIC",
                    "-o",
                    dll_path,
                    dll_cpp_path,
                ]
                
                result = self._run_step(f"Build-DLL({system_name})", dll_build_cmd)
                if result:
                    self.log_lines.append(f"  DLL built: {dll_name}")
                else:
                    self.log_lines.append(f"  ERROR: Failed to build DLL {dll_name}")
        
        return shader_compile_time
    
    def hotload_dll(self):
        """Rebuild hot-reloadable DLL(s) for @hot systems, compile hot shaders, or recompile resources."""
        if not self.has_hot_systems():
            self.status = "No @hot systems, shaders, or resources found"
            self.log_lines.append("ERROR: No @hot systems, shaders, or resources found in current file")
            return
        
        self.save()
        hd_path = os.path.abspath(self.original_hd_path)
        project_dir = os.path.dirname(hd_path)
        
        # Check if we have resources (need to recompile HEIDIC code)
        has_resources = self.has_resources()
        
        # Step 1: Compile hot shaders (GLSL -> SPIR-V)
        hot_shaders = self.find_hot_shaders()
        if hot_shaders:
            self.status = "Hotloading: Compiling shaders..."
            pygame.display.flip()
            glslc_found = False
            glslc_path = None
            
            # Try to find glslc (from Vulkan SDK)
            vulkan_sdk = os.environ.get("VULKAN_SDK")
            if vulkan_sdk:
                glslc_path = os.path.join(vulkan_sdk, "bin", "glslc.exe")
                if os.path.exists(glslc_path):
                    glslc_found = True
            
            # Try system PATH
            if not glslc_found:
                import shutil
                glslc_path = shutil.which("glslc")
                if glslc_path:
                    glslc_found = True
            
            if glslc_found:
                for glsl_path in hot_shaders:
                    # Determine output path (.spv) - keep extension to avoid conflicts
                    if glsl_path.endswith('.glsl'):
                        spv_path = glsl_path[:-5] + '.spv'
                    elif glsl_path.endswith('.vert'):
                        spv_path = glsl_path + '.spv'  # my_shader.vert.spv
                    elif glsl_path.endswith('.frag'):
                        spv_path = glsl_path + '.spv'  # my_shader.frag.spv
                    elif glsl_path.endswith('.comp'):
                        spv_path = glsl_path + '.spv'  # my_shader.comp.spv
                    else:
                        spv_path = glsl_path + '.spv'
                    
                    # Determine shader stage for glslc
                    stage_flag = None
                    if glsl_path.endswith('.comp'):
                        stage_flag = "-fshader-stage=compute"
                    elif glsl_path.endswith('.vert'):
                        stage_flag = "-fshader-stage=vertex"
                    elif glsl_path.endswith('.frag'):
                        stage_flag = "-fshader-stage=fragment"
                    elif glsl_path.endswith('.geom'):
                        stage_flag = "-fshader-stage=geometry"
                    
                    # Compile shader
                    compile_cmd = [glslc_path]
                    if stage_flag:
                        compile_cmd.append(stage_flag)
                    compile_cmd.extend([glsl_path, "-o", spv_path])
                    
                    result = self._run_step(f"Hotload-CompileShader({os.path.basename(glsl_path)})", compile_cmd)
                    if result:
                        self.log_lines.append(f"Hotload: Compiled shader {os.path.basename(glsl_path)} -> {os.path.basename(spv_path)}")
                    else:
                        self.log_lines.append(f"WARNING: Failed to compile shader {os.path.basename(glsl_path)}")
            else:
                self.log_lines.append("WARNING: glslc not found. Shader compilation skipped.")
                self.log_lines.append("WARNING: Install Vulkan SDK or ensure glslc is in PATH.")
        
        # Step 2: Recompile HEIDIC source (if we have hot systems OR resources)
        dll_files = self.find_hot_dll_files()
        if dll_files or has_resources:
            self.status = "Hotloading: Recompiling HEIDIC..."
            pygame.display.flip()
            result = self._run_step("Hotload-Compile", [p.format(file=hd_path, cpp="") for p in CMD_COMPILE])
            
            if not result:
                self.status = "Hotload failed: Compilation error"
                return
        
        # Step 3: Handle resources (recompile and rebuild executable)
        if has_resources and not dll_files:
            # For resources, we need to rebuild the executable to load new resource declarations
            # NOTE: The game must be stopped first, otherwise the .exe file will be locked
            self.status = "Hotloading: Rebuilding executable for resources..."
            pygame.display.flip()
            
            # Use the same build logic as regular build
            cpp_file = hd_path.replace(".hd", ".cpp")
            if os.path.exists(cpp_file):
                # Rebuild using _do_build (it handles everything: shaders, compilation, linking)
                # Check if build succeeded by checking if exe was created/updated
                exe_name = os.path.splitext(os.path.basename(hd_path))[0] + ".exe"
                exe_path = os.path.join(os.path.dirname(hd_path), exe_name)
                exe_existed_before = os.path.exists(exe_path)
                
                self._do_build(hd_path, cpp_file)
                
                # Check if build succeeded
                if os.path.exists(exe_path):
                    self.status = "Hotload: Resources recompiled successfully"
                    self.log_lines.append("Hotload: Resources recompiled - restart the game to see changes")
                    self.log_lines.append("NOTE: Stop the game first if it's running, then restart to see new resources")
                else:
                    self.status = "Hotload failed: Could not rebuild executable"
                    self.log_lines.append("ERROR: Build failed - make sure the game is stopped (exe file may be locked)")
                return  # Resources handled, we're done
            else:
                self.log_lines.append("WARNING: C++ file not found, cannot rebuild for resources")
                return
        
        # Step 4: Find and rebuild DLL files (only if we have hot systems)
        if not dll_files and not hot_shaders:
            # No hot systems, no hot shaders, and no resources (or resources already handled above)
            self.status = "Hotload: No hot-reloadable items found"
            self.log_lines.append("WARNING: No @hot systems, shaders, or resources to hotload")
            return
        
        if not dll_files:
            # Only shaders, no systems - just report success
            self.status = "Hotload: Shaders compiled successfully"
            return
        
        project_dir = os.path.dirname(hd_path)
        self.status = "Hotloading: Rebuilding DLL(s)..."
        pygame.display.flip()
        
        any_replaced = False
        for dll_cpp_path in dll_files:
            # Extract system name from filename (e.g., "rotation_hot.dll.cpp" -> "rotation")
            dll_cpp_name = os.path.basename(dll_cpp_path)
            system_name = dll_cpp_name.replace("_hot.dll.cpp", "")
            dll_name = f"{system_name}.dll"
            dll_path = os.path.join(project_dir, dll_name)
            dll_new_path = os.path.join(project_dir, f"{dll_name}.new")
            
            # Compile DLL to temporary file
            compile_cmd = [
                "g++",
                "-std=c++17",
                "-shared",
                "-fPIC",
                "-o",
                dll_new_path,
                dll_cpp_path,
            ]
            
            result = self._run_step(f"Hotload-CompileDLL({system_name})", compile_cmd)
            if not result:
                continue  # Try next DLL if this one fails
            
            # Try to replace DLL with retry logic
            # Strategy: Try to rename old DLL to .old, then rename new DLL to final name
            replaced = False
            max_retries = 15  # More retries since game needs time to detect and unload
            import time
            dll_old_path = os.path.join(project_dir, f"{dll_name}.old")
            
            for attempt in range(max_retries):
                try:
                    # First, try to move/rename the old DLL to .old (if it exists)
                    if os.path.exists(dll_path):
                        try:
                            # Try to rename old DLL to .old (this will fail if DLL is locked)
                            if os.path.exists(dll_old_path):
                                os.remove(dll_old_path)  # Remove old backup if exists
                            os.rename(dll_path, dll_old_path)
                            # Successfully moved old DLL, small delay
                            time.sleep(0.1)
                        except (PermissionError, OSError) as e:
                            # DLL is still loaded, wait for game to unload it
                            if attempt < max_retries - 1:
                                wait_time = 1.0 + (attempt * 0.2)  # Increasing wait time
                                self.log_lines.append(f"Hotload: DLL locked, waiting {wait_time:.1f}s for game to unload... (attempt {attempt + 1}/{max_retries})")
                                time.sleep(wait_time)
                                continue
                            else:
                                self.log_lines.append(f"Hotload: DLL still locked after {max_retries} attempts")
                                self.log_lines.append(f"Hotload: Make sure the game is running and will detect DLL changes")
                                break
                    
                    # Now try to rename the new DLL to the final name
                    if os.path.exists(dll_new_path):
                        try:
                            os.rename(dll_new_path, dll_path)
                            if not os.path.exists(dll_new_path) and os.path.exists(dll_path):
                                replaced = True
                                any_replaced = True
                                self.log_lines.append(f"Hotload: {dll_name} rebuilt successfully")
                                # Clean up old backup if it exists
                                if os.path.exists(dll_old_path):
                                    try:
                                        os.remove(dll_old_path)
                                    except:
                                        pass  # Ignore cleanup errors
                                break
                        except Exception as e:
                            if attempt < max_retries - 1:
                                wait_time = 1.0
                                self.log_lines.append(f"Hotload: Error renaming DLL: {e}, retrying in {wait_time}s...")
                                time.sleep(wait_time)
                                continue
                            else:
                                self.log_lines.append(f"Hotload: Failed to rename DLL: {e}")
                                break
                    else:
                        self.log_lines.append(f"Hotload: New DLL file not found: {dll_new_path}")
                        break
                
                except Exception as e:
                    if attempt < max_retries - 1:
                        time.sleep(1)
                        self.log_lines.append(f"Hotload: Unexpected error: {e}, retrying...")
                    else:
                        self.log_lines.append(f"Hotload: Failed to replace {dll_name}: {e}")
            
            if not replaced:
                self.log_lines.append(f"ERROR: Could not replace {dll_name} (still locked after {max_retries} attempts)")
        
        if any_replaced:
            self.status = "Hotload: DLL(s) rebuilt successfully"
        else:
            self.status = "Hotload: Some DLLs may still be locked"

    def _run_step(self, label, cmd, capture_output=True):
        try:
            proc = subprocess.run(
                cmd,
                cwd=os.path.abspath(os.path.join(os.path.dirname(__file__), "..")),
                capture_output=capture_output,
                text=True,
                shell=False,
            )
            if proc.returncode != 0:
                out = (proc.stdout or "") + (proc.stderr or "")
                self.status = f"{label} failed: {proc.returncode}"
                if out:
                    self.log_lines.extend(out.splitlines())
                return False
            if capture_output:
                self.status = f"{label} ok"
                if proc.stdout:
                    self.log_lines.extend(proc.stdout.splitlines())
                if proc.stderr:
                    self.log_lines.extend(proc.stderr.splitlines())
            else:
                self.status = f"{label} launched"
                self.log_lines.append(f"{label}: launched {' '.join(cmd)}")
            return True
        except FileNotFoundError:
            self.status = f"{label} failed: cmd not found"
            self.log_lines.append(f"{label}: command not found")
            return False


def text_input_dialog(screen, editor, prompt, initial_text=""):
    """Simple pygame text input dialog. Returns the entered text or None if cancelled.
    Note: For project names, .hd extension is NOT added automatically."""
    """Simple pygame text input dialog. Returns the entered text or None if cancelled."""
    font = pygame.font.SysFont("Consolas", 16)
    dialog_width = 500
    dialog_height = 120
    dialog_x = (SCREEN_WIDTH - dialog_width) // 2
    dialog_y = (SCREEN_HEIGHT - dialog_height) // 2
    
    text = initial_text
    cursor_pos = len(text)
    clock = pygame.time.Clock()
    cursor_visible = True
    cursor_timer = 0
    
    running = True
    while running:
        cursor_timer += clock.tick(60)
        if cursor_timer > 500:
            cursor_visible = not cursor_visible
            cursor_timer = 0
        
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                return None
            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_RETURN:
                    return text
                elif event.key == pygame.K_ESCAPE:
                    return None
                elif event.key == pygame.K_BACKSPACE:
                    if cursor_pos > 0:
                        text = text[:cursor_pos-1] + text[cursor_pos:]
                        cursor_pos -= 1
                elif event.key == pygame.K_LEFT:
                    cursor_pos = max(0, cursor_pos - 1)
                elif event.key == pygame.K_RIGHT:
                    cursor_pos = min(len(text), cursor_pos + 1)
                elif event.key == pygame.K_HOME:
                    cursor_pos = 0
                elif event.key == pygame.K_END:
                    cursor_pos = len(text)
                elif event.unicode and event.unicode.isprintable():
                    text = text[:cursor_pos] + event.unicode + text[cursor_pos:]
                    cursor_pos += 1
        
        # Redraw editor background
        editor.draw(screen)
        
        # Draw semi-transparent overlay
        overlay = pygame.Surface((SCREEN_WIDTH, SCREEN_HEIGHT))
        overlay.set_alpha(180)
        overlay.fill((0, 0, 0))
        screen.blit(overlay, (0, 0))
        
        # Draw dialog
        dialog_surface = pygame.Surface((dialog_width, dialog_height))
        dialog_surface.fill((50, 50, 50))
        pygame.draw.rect(dialog_surface, (70, 70, 70), pygame.Rect(0, 0, dialog_width, dialog_height), 2)
        
        # Prompt
        prompt_surf = font.render(prompt, True, (240, 240, 240))
        dialog_surface.blit(prompt_surf, (10, 10))
        
        # Text input box
        input_rect = pygame.Rect(10, 40, dialog_width - 20, 30)
        pygame.draw.rect(dialog_surface, (30, 30, 30), input_rect)
        pygame.draw.rect(dialog_surface, (100, 150, 255), input_rect, 2)
        
        # Text
        text_surf = font.render(text, True, (240, 240, 240))
        dialog_surface.blit(text_surf, (input_rect.x + 5, input_rect.y + 5))
        
        # Cursor
        if cursor_visible:
            cursor_x = input_rect.x + 5 + font.size(text[:cursor_pos])[0]
            pygame.draw.line(dialog_surface, (240, 240, 240), (cursor_x, input_rect.y + 5), (cursor_x, input_rect.y + 25), 2)
        
        # Instructions
        hint_surf = font.render("Enter: Confirm | Esc: Cancel", True, (150, 150, 150))
        dialog_surface.blit(hint_surf, (10, dialog_height - 25))
        
        screen.blit(dialog_surface, (dialog_x, dialog_y))
        pygame.display.flip()
    
    return None


def project_picker_dialog(screen, editor):
    """Simple project picker that lists all .hd files in SCRIBE/PROJECTS/"""
    font = pygame.font.SysFont("Consolas", 16)
    dialog_width = 600
    dialog_height = 500
    dialog_x = (SCREEN_WIDTH - dialog_width) // 2
    dialog_y = (SCREEN_HEIGHT - dialog_height) // 2
    
    # Find all .hd files in PROJECTS folder
    projects_dir = os.path.join(os.path.dirname(__file__), "PROJECTS")
    projects = []
    if os.path.exists(projects_dir):
        for root, dirs, files in os.walk(projects_dir):
            for file in files:
                if file.endswith(".hd"):
                    full_path = os.path.join(root, file)
                    rel_path = os.path.relpath(full_path, projects_dir)
                    projects.append((rel_path, full_path))
    projects.sort()
    
    if not projects:
        # Show "no projects" message briefly
        return None
    
    selected_idx = 0
    scroll_offset = 0
    items_per_page = 20
    item_height = 24
    clock = pygame.time.Clock()
    
    running = True
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                return None
            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_RETURN:
                    if projects:
                        return projects[selected_idx][1]  # Return full path
                elif event.key == pygame.K_ESCAPE:
                    return None
                elif event.key == pygame.K_UP:
                    selected_idx = max(0, selected_idx - 1)
                    if selected_idx < scroll_offset:
                        scroll_offset = selected_idx
                elif event.key == pygame.K_DOWN:
                    selected_idx = min(len(projects) - 1, selected_idx + 1)
                    if selected_idx >= scroll_offset + items_per_page:
                        scroll_offset = selected_idx - items_per_page + 1
                elif event.key == pygame.K_PAGEUP:
                    selected_idx = max(0, selected_idx - items_per_page)
                    scroll_offset = max(0, scroll_offset - items_per_page)
                elif event.key == pygame.K_PAGEDOWN:
                    selected_idx = min(len(projects) - 1, selected_idx + items_per_page)
                    scroll_offset = min(max(0, len(projects) - items_per_page), scroll_offset + items_per_page)
        
        # Redraw editor background
        editor.draw(screen)
        
        # Draw semi-transparent overlay
        overlay = pygame.Surface((SCREEN_WIDTH, SCREEN_HEIGHT))
        overlay.set_alpha(180)
        overlay.fill((0, 0, 0))
        screen.blit(overlay, (0, 0))
        
        # Draw dialog
        dialog_surface = pygame.Surface((dialog_width, dialog_height))
        dialog_surface.fill((50, 50, 50))
        pygame.draw.rect(dialog_surface, (70, 70, 70), pygame.Rect(0, 0, dialog_width, dialog_height), 2)
        
        # Title
        title_surf = font.render("Load Project from PROJECTS folder:", True, (240, 240, 240))
        dialog_surface.blit(title_surf, (10, 10))
        
        # List area
        list_rect = pygame.Rect(10, 40, dialog_width - 20, dialog_height - 80)
        pygame.draw.rect(dialog_surface, (30, 30, 30), list_rect)
        pygame.draw.rect(dialog_surface, (100, 150, 255), list_rect, 2)
        
        # Draw projects list
        visible_start = scroll_offset
        visible_end = min(len(projects), scroll_offset + items_per_page)
        for i in range(visible_start, visible_end):
            idx = i
            rel_path, _ = projects[idx]
            y_pos = list_rect.y + 5 + (i - scroll_offset) * item_height
            
            # Highlight selected
            if idx == selected_idx:
                highlight_rect = pygame.Rect(list_rect.x + 2, y_pos - 2, list_rect.width - 4, item_height)
                pygame.draw.rect(dialog_surface, (70, 130, 200), highlight_rect)
            
            # Project name
            text_surf = font.render(rel_path, True, (240, 240, 240))
            dialog_surface.blit(text_surf, (list_rect.x + 5, y_pos))
        
        # Instructions
        hint_surf = font.render("↑↓: Select | Enter: Load | Esc: Cancel", True, (150, 150, 150))
        dialog_surface.blit(hint_surf, (10, dialog_height - 30))
        
        screen.blit(dialog_surface, (dialog_x, dialog_y))
        pygame.display.flip()
        clock.tick(60)
    
    return None


def shader_picker_dialog(screen, editor):
    """Simple shader picker that lists all shader files in the current project"""
    if not editor.original_hd_path or not editor.original_hd_path.endswith(".hd"):
        return None
    
    shaders = editor.find_shaders_in_project()
    if not shaders:
        return None
    
    font = pygame.font.SysFont("Consolas", 16)
    dialog_width = 600
    dialog_height = 400
    dialog_x = (SCREEN_WIDTH - dialog_width) // 2
    dialog_y = (SCREEN_HEIGHT - dialog_height) // 2
    
    selected_idx = 0
    scroll_offset = 0
    items_per_page = 15
    item_height = 24
    clock = pygame.time.Clock()
    
    running = True
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                return None
            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_RETURN:
                    if shaders:
                        return shaders[selected_idx][1]  # Return full path
                elif event.key == pygame.K_ESCAPE:
                    return None
                elif event.key == pygame.K_UP:
                    selected_idx = max(0, selected_idx - 1)
                    if selected_idx < scroll_offset:
                        scroll_offset = selected_idx
                elif event.key == pygame.K_DOWN:
                    selected_idx = min(len(shaders) - 1, selected_idx + 1)
                    if selected_idx >= scroll_offset + items_per_page:
                        scroll_offset = selected_idx - items_per_page + 1
                elif event.key == pygame.K_PAGEUP:
                    selected_idx = max(0, selected_idx - items_per_page)
                    scroll_offset = max(0, scroll_offset - items_per_page)
                elif event.key == pygame.K_PAGEDOWN:
                    selected_idx = min(len(shaders) - 1, selected_idx + items_per_page)
                    scroll_offset = min(max(0, len(shaders) - items_per_page), scroll_offset + items_per_page)
        
        # Redraw editor background
        editor.draw(screen)
        
        # Draw semi-transparent overlay
        overlay = pygame.Surface((SCREEN_WIDTH, SCREEN_HEIGHT))
        overlay.set_alpha(180)
        overlay.fill((0, 0, 0))
        screen.blit(overlay, (0, 0))
        
        # Draw dialog
        dialog_surface = pygame.Surface((dialog_width, dialog_height))
        dialog_surface.fill((50, 50, 50))
        pygame.draw.rect(dialog_surface, (70, 70, 70), pygame.Rect(0, 0, dialog_width, dialog_height), 2)
        
        # Title
        title_surf = font.render("Load Shader from Project:", True, (240, 240, 240))
        dialog_surface.blit(title_surf, (10, 10))
        
        # List area
        list_rect = pygame.Rect(10, 40, dialog_width - 20, dialog_height - 80)
        pygame.draw.rect(dialog_surface, (30, 30, 30), list_rect)
        pygame.draw.rect(dialog_surface, (100, 150, 255), list_rect, 2)
        
        # Draw shaders list
        visible_start = scroll_offset
        visible_end = min(len(shaders), scroll_offset + items_per_page)
        for i in range(visible_start, visible_end):
            idx = i
            rel_path, _ = shaders[idx]
            y_pos = list_rect.y + 5 + (i - scroll_offset) * item_height
            
            # Highlight selected
            if idx == selected_idx:
                highlight_rect = pygame.Rect(list_rect.x + 2, y_pos - 2, list_rect.width - 4, item_height)
                pygame.draw.rect(dialog_surface, (70, 130, 200), highlight_rect)
            
            # Shader name
            text_surf = font.render(rel_path, True, (240, 240, 240))
            dialog_surface.blit(text_surf, (list_rect.x + 5, y_pos))
        
        # Instructions
        hint_surf = font.render("↑↓: Select | Enter: Load | Esc: Cancel", True, (150, 150, 150))
        dialog_surface.blit(hint_surf, (10, dialog_height - 30))
        
        screen.blit(dialog_surface, (dialog_x, dialog_y))
        pygame.display.flip()
        clock.tick(60)
    
    return None


def main():
    # Start with empty editor or load from command line
    path = DEFAULT_FILE
    if len(sys.argv) > 1:
        path = sys.argv[1]
    elif not path:
        # No default - start with empty state, user must create/load project
        script_dir = os.path.dirname(__file__)
        path = os.path.join(script_dir, "empty.hd")
        # Create empty file if it doesn't exist
        if not os.path.exists(path):
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "w", encoding="utf-8") as f:
                f.write("// No project loaded. Use New Project or Load Project from the menu.\n")
    
    pygame.init()
    screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT))
    pygame.display.set_caption("SCRIBE - HEIDIC Script Editor")
    clock = pygame.time.Clock()
    editor = Editor(path)

    running = True
    while running:
        # Update menu hitboxes once per frame (needed for mouse handling)
        editor._build_menu_hitboxes()
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
                mx, my = event.pos
                offset_x = SCREEN_WIDTH - EDITOR_WIDTH
                local_x = mx - offset_x
                local_y = my
                # Left panel interactions (logs and terminal)
                if mx < SCREEN_WIDTH - EDITOR_WIDTH:
                    panel_width = SCREEN_WIDTH - EDITOR_WIDTH
                    log_height = SCREEN_HEIGHT // 2
                    terminal_height = SCREEN_HEIGHT - log_height
                    terminal_y = log_height
                    
                    # Determine which section was clicked
                    if my < log_height:
                        # LOG SECTION
                        v_sb = editor.calc_log_vscrollbar(panel_width, log_height)
                        h_sb = editor.calc_log_hscrollbar(panel_width, log_height, 0)
                        # Log copy button
                        copy_btn_rect = editor.log_copy_btn_rect
                        if copy_btn_rect.collidepoint(mx, my):
                            try:
                                log_text = "\n".join(editor.log_lines) if editor.log_lines else ""
                                if log_text:
                                    # Use Windows clip command (most reliable on Windows)
                                    try:
                                        # Use shell=True and proper encoding
                                        proc = subprocess.Popen(
                                            'clip',
                                            stdin=subprocess.PIPE,
                                            stdout=subprocess.PIPE,
                                            stderr=subprocess.PIPE,
                                            shell=True,
                                            creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == 'win32' else 0
                                        )
                                        # Write text and close stdin
                                        proc.stdin.write(log_text.encode('utf-8', errors='replace'))
                                        proc.stdin.close()
                                        proc.wait(timeout=2)
                                        if proc.returncode == 0:
                                            editor.status = "Copied log to clipboard"
                                        else:
                                            raise Exception(f"Clip command failed with code {proc.returncode}")
                                    except Exception as clip_error:
                                        # Fallback to pyperclip if available, then tkinter
                                        try:
                                            import pyperclip
                                            pyperclip.copy(log_text)
                                            editor.status = "Copied log to clipboard"
                                        except ImportError:
                                            # Final fallback to tkinter
                                            import tkinter as tk
                                            root = tk.Tk()
                                            root.withdraw()
                                            root.clipboard_clear()
                                            root.clipboard_append(log_text)
                                            root.update()
                                            # Keep root alive long enough for clipboard to update
                                            root.after(150, root.destroy)
                                            editor.status = "Copied log to clipboard (tkinter)"
                                else:
                                    editor.status = "Log is empty"
                            except Exception as e:
                                editor.status = f"Copy failed: {str(e)}"
                        # Log scrollbars
                        if v_sb and LOG_HEADER_HEIGHT <= my < log_height:
                            sb_rect = pygame.Rect(
                                v_sb["scrollbar_x"],
                                v_sb["scrollbar_y"],
                                LOG_SCROLLBAR_WIDTH,
                                v_sb["usable_height"],
                            )
                            bar_rect = pygame.Rect(
                                sb_rect.x,
                                sb_rect.y + v_sb["bar_pos"],
                                sb_rect.w,
                                v_sb["bar_height"],
                            )
                            if bar_rect.collidepoint(mx, my):
                                editor.dragging_log_v = True
                                editor._log_drag_offset = my - bar_rect.y
                            elif sb_rect.collidepoint(mx, my):
                                rel = my - sb_rect.y
                                target = max(0, min(v_sb["usable_height"] - v_sb["bar_height"], rel - v_sb["bar_height"] // 2))
                                if (v_sb["usable_height"] - v_sb["bar_height"]) > 0:
                                    editor.log_vscroll = int((target / (v_sb["usable_height"] - v_sb["bar_height"])) * v_sb["max_scroll"])
                                else:
                                    editor.log_vscroll = 0
                        if h_sb:
                            hsb_rect = pygame.Rect(
                                h_sb["track_x"],
                                h_sb["track_y"],
                                h_sb["usable_width"],
                                LOG_HSCROLL_HEIGHT,
                            )
                            thumb_rect = pygame.Rect(
                                h_sb["track_x"] + h_sb["bar_pos"],
                                h_sb["track_y"],
                                h_sb["bar_width"],
                                LOG_HSCROLL_HEIGHT,
                            )
                            if thumb_rect.collidepoint(mx, my):
                                editor.dragging_log_h = True
                                editor._log_h_drag_offset = mx - thumb_rect.x
                            elif hsb_rect.collidepoint(mx, my):
                                rel = mx - hsb_rect.x
                                target = max(0, min(h_sb["usable_width"] - h_sb["bar_width"], rel - h_sb["bar_width"] // 2))
                                if (h_sb["usable_width"] - h_sb["bar_width"]) > 0:
                                    editor.log_hscroll = int((target / (h_sb["usable_width"] - h_sb["bar_width"])) * h_sb["max_scroll"])
                                else:
                                    editor.log_hscroll = 0
                    else:
                        # TERMINAL SECTION (read-only output display)
                        v_sb_term = editor.calc_terminal_vscrollbar(panel_width, terminal_height)
                        h_sb_term = editor.calc_terminal_hscrollbar(panel_width, terminal_height, terminal_y)
                        # Terminal copy button
                        terminal_copy_btn = pygame.Rect(MARGIN, terminal_y + (TERMINAL_HEADER_HEIGHT - 22) // 2, TERMINAL_COPY_BTN_WIDTH, 22)
                        if terminal_copy_btn.collidepoint(mx, my):
                            try:
                                term_text = "\n".join(editor.terminal_lines) if editor.terminal_lines else ""
                                if term_text:
                                    # Use Windows clip command (most reliable on Windows)
                                    try:
                                        # Use shell=True and proper encoding
                                        proc = subprocess.Popen(
                                            'clip',
                                            stdin=subprocess.PIPE,
                                            stdout=subprocess.PIPE,
                                            stderr=subprocess.PIPE,
                                            shell=True,
                                            creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == 'win32' else 0
                                        )
                                        # Write text and close stdin
                                        proc.stdin.write(term_text.encode('utf-8', errors='replace'))
                                        proc.stdin.close()
                                        proc.wait(timeout=2)
                                        if proc.returncode == 0:
                                            editor.status = "Copied terminal to clipboard"
                                        else:
                                            raise Exception(f"Clip command failed with code {proc.returncode}")
                                    except Exception as clip_error:
                                        # Fallback to pyperclip if available, then tkinter
                                        try:
                                            import pyperclip
                                            pyperclip.copy(term_text)
                                            editor.status = "Copied terminal to clipboard"
                                        except ImportError:
                                            # Final fallback to tkinter
                                            import tkinter as tk
                                            root = tk.Tk()
                                            root.withdraw()
                                            root.clipboard_clear()
                                            root.clipboard_append(term_text)
                                            root.update()
                                            # Keep root alive long enough for clipboard to update
                                            root.after(150, root.destroy)
                                            editor.status = "Copied terminal to clipboard (tkinter)"
                                else:
                                    editor.status = "Terminal is empty"
                            except Exception as e:
                                editor.status = f"Copy failed: {str(e)}"
                        # Terminal scrollbars
                        if v_sb_term and terminal_y + TERMINAL_HEADER_HEIGHT <= my < SCREEN_HEIGHT:
                            scrollbar_y_pos = terminal_y + TERMINAL_HEADER_HEIGHT + MARGIN
                            sb_rect = pygame.Rect(
                                v_sb_term["scrollbar_x"],
                                scrollbar_y_pos,
                                LOG_SCROLLBAR_WIDTH,
                                v_sb_term["usable_height"],
                            )
                            bar_rect = pygame.Rect(
                                sb_rect.x,
                                sb_rect.y + v_sb_term["bar_pos"],
                                sb_rect.w,
                                v_sb_term["bar_height"],
                            )
                            if bar_rect.collidepoint(mx, my):
                                editor.dragging_terminal_v = True
                                editor._terminal_drag_offset = my - bar_rect.y
                            elif sb_rect.collidepoint(mx, my):
                                rel = my - sb_rect.y
                                target = max(0, min(v_sb_term["usable_height"] - v_sb_term["bar_height"], rel - v_sb_term["bar_height"] // 2))
                                if (v_sb_term["usable_height"] - v_sb_term["bar_height"]) > 0:
                                    editor.terminal_vscroll = int((target / (v_sb_term["usable_height"] - v_sb_term["bar_height"])) * v_sb_term["max_scroll"])
                                else:
                                    editor.terminal_vscroll = 0
                        if h_sb_term:
                            hsb_rect = pygame.Rect(
                                h_sb_term["track_x"],
                                h_sb_term["track_y"],
                                h_sb_term["usable_width"],
                                LOG_HSCROLL_HEIGHT,
                            )
                            thumb_rect = pygame.Rect(
                                hsb_rect.x + h_sb_term["bar_pos"],
                                hsb_rect.y,
                                h_sb_term["bar_width"],
                                LOG_HSCROLL_HEIGHT,
                            )
                            if thumb_rect.collidepoint(mx, my):
                                editor.dragging_terminal_h = True
                                editor._terminal_h_drag_offset = mx - thumb_rect.x
                            elif hsb_rect.collidepoint(mx, my):
                                rel = mx - hsb_rect.x
                                target = max(0, min(h_sb_term["usable_width"] - h_sb_term["bar_width"], rel - h_sb_term["bar_width"] // 2))
                                if (h_sb_term["usable_width"] - h_sb_term["bar_width"]) > 0:
                                    editor.terminal_hscroll = int((target / (h_sb_term["usable_width"] - h_sb_term["bar_width"])) * h_sb_term["max_scroll"])
                                else:
                                    editor.terminal_hscroll = 0
                if 0 <= local_y < MENU_HEIGHT and 0 <= local_x < EDITOR_WIDTH:
                    for rect, icon_char, tooltip, action in editor.menu_hitboxes:
                        if rect.collidepoint(local_x, local_y):
                            if action == "new":
                                project_name = text_input_dialog(screen, editor, "New HEIDIC Project (name):")
                                if project_name:
                                    # Create project folder in SCRIBE/PROJECTS/
                                    project_dir = os.path.join(os.path.dirname(__file__), "PROJECTS", project_name)
                                    os.makedirs(project_dir, exist_ok=True)
                                    # Create shaders and textures folders
                                    shaders_dir = os.path.join(project_dir, "shaders")
                                    textures_dir = os.path.join(project_dir, "textures")
                                    os.makedirs(shaders_dir, exist_ok=True)
                                    os.makedirs(textures_dir, exist_ok=True)
                                    # Create .hd file with same name as project
                                    hd_filename = project_name + ".hd"
                                    new_path = os.path.join(project_dir, hd_filename)
                                    # Template with Vulkan rendering setup
                                    # All projects now support shader hotloading
                                    if project_name.lower() == "hotload_test":
                                        # Create hot-reload template
                                        template = f"""// HEIDIC Project: {project_name}
// Hot-reloadable project with Vulkan rendering

extern fn heidic_glfw_vulkan_hints(): void;
extern fn heidic_init_renderer(window: GLFWwindow): i32;
extern fn heidic_render_frame(window: GLFWwindow): void;
extern fn heidic_set_rotation_speed(speed: f32): void;
extern fn heidic_cleanup_renderer(): void;
extern fn heidic_sleep_ms(milliseconds: i32): void;

// Hot-reloadable system for rotation
// Edit rotation_speed and use Hotload button to see changes instantly!
@hot
system(rotation) {{
    fn get_rotation_speed(): f32 {{
        // Try changing this value while the game is running!
        let rotation_speed: f32 = 1.0;
        return rotation_speed;
    }}
}}

fn main(): void {{
    print("=== {project_name} ===\\n");
    print("Initializing GLFW...\\n");

    let init_result: i32 = glfwInit();
    if init_result == 0 {{
        print("Failed to initialize GLFW!\\n");
        return;
    }}

    print("GLFW initialized.\\n");
    heidic_glfw_vulkan_hints();
    
    print("Creating window (800x600)...\\n");
    let window: GLFWwindow = glfwCreateWindow(800, 600, "{project_name} - Hot Reload Test", 0, 0);
    if window == 0 {{
        print("Failed to create window!\\n");
        glfwTerminate();
        return;
    }}

    print("Window created.\\n");
    print("Initializing Vulkan renderer...\\n");

    let renderer_init: i32 = heidic_init_renderer(window);
    if renderer_init == 0 {{
        print("Failed to initialize renderer!\\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return;
    }}

    print("Renderer initialized!\\n");
    print("Starting render loop...\\n");
    print("\\n");
    print("=== HOT-RELOAD INSTRUCTIONS ===\\n");
    print("1. Edit rotation_speed in the @hot system\\n");
    print("2. Save the file\\n");
    print("3. Click the red Hotload button (▶)\\n");
    print("4. Watch the triangle spin faster/slower!\\n");
    print("5. Press ESC or close window to exit\\n");
    print("\\n");

    while glfwWindowShouldClose(window) == 0 {{
        glfwPollEvents();

        if glfwGetKey(window, 256) == 1 {{ // ESC key
            glfwSetWindowShouldClose(window, 1);
        }}

        // Get rotation speed from hot-reloadable system
        let rotation_speed: f32 = get_rotation_speed();
        heidic_set_rotation_speed(rotation_speed); // pass speed to renderer for live updates
        
        heidic_render_frame(window);
        heidic_sleep_ms(16); // ~60 FPS cap
    }}

    print("Cleaning up...\\n");
    heidic_cleanup_renderer();
    glfwDestroyWindow(window);
    glfwTerminate();
    print("Program exited successfully.\\n");
    print("Done!\\n");
}}
"""
                                    elif project_name.lower() == "blank_screen":
                                        # Blank screen template (no rendering, just initialization)
                                        template = f"""// HEIDIC Project: {project_name}
// Blank screen template - initialize renderer but don't render anything

extern fn heidic_glfw_vulkan_hints(): void;
extern fn heidic_init_renderer(window: GLFWwindow): i32;
extern fn heidic_cleanup_renderer(): void;
extern fn heidic_sleep_ms(milliseconds: i32): void;

fn main(): void {{
    print("=== {project_name} ===\\n");
    print("Initializing GLFW...\\n");

    let init_result: i32 = glfwInit();
    if init_result == 0 {{
        print("Failed to initialize GLFW!\\n");
        return;
    }}

    print("GLFW initialized.\\n");
    heidic_glfw_vulkan_hints();
    
    print("Creating window (800x600)...\\n");
    let window: GLFWwindow = glfwCreateWindow(800, 600, "{project_name}", 0, 0);
    if window == 0 {{
        print("Failed to create window!\\n");
        glfwTerminate();
        return;
    }}

    print("Window created.\\n");
    print("Initializing Vulkan renderer...\\n");

    let renderer_init: i32 = heidic_init_renderer(window);
    if renderer_init == 0 {{
        print("Failed to initialize renderer!\\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return;
    }}

    print("Renderer initialized!\\n");
    print("Window is open (blank screen).\\n");
    print("Press ESC or close the window to exit.\\n");

    while glfwWindowShouldClose(window) == 0 {{
        glfwPollEvents();

        if glfwGetKey(window, 256) == 1 {{ // ESC key
            glfwSetWindowShouldClose(window, 1);
        }}

        // No rendering - just blank screen
        heidic_sleep_ms(16); // ~60 FPS cap
    }}

    print("Cleaning up...\\n");
    heidic_cleanup_renderer();
    glfwDestroyWindow(window);
    glfwTerminate();
    print("Program exited successfully.\\n");
    print("Done!\\n");
}}
"""
                                    else:
                                        # All projects support shader hotloading - use default template
                                        template = f"""// HEIDIC Project: {project_name}
// This project supports shader hotloading - add @hot shader declarations to use it!

extern fn heidic_glfw_vulkan_hints(): void;
extern fn heidic_init_renderer(window: GLFWwindow): i32;
extern fn heidic_render_frame(window: GLFWwindow): void;
extern fn heidic_set_rotation_speed(speed: f32): void;
extern fn heidic_cleanup_renderer(): void;
extern fn heidic_sleep_ms(milliseconds: i32): void;

extern fn heidic_glfw_vulkan_hints(): void;
extern fn heidic_init_renderer(window: GLFWwindow): i32;
extern fn heidic_render_frame(window: GLFWwindow): void;
extern fn heidic_set_rotation_speed(speed: f32): void;
extern fn heidic_cleanup_renderer(): void;
extern fn heidic_sleep_ms(milliseconds: i32): void;

// Hot-reloadable system - edit values and use Hotload button to see changes instantly!
// To add shader hotloading, add @hot shader declarations:
// @hot
// shader vertex "shaders/my_shader.vert" {{}}
// @hot
// shader fragment "shaders/my_shader.frag" {{}}
@hot
system(game_state) {{
    fn get_rotation_speed(): f32 {{
        // Try changing this value while the game is running and use Hotload!
        let rotation_speed: f32 = 1.0;
        return rotation_speed;
    }}
}}

fn main(): void {{
    print("=== {project_name} ===\\n");
    print("Initializing GLFW...\\n");

    let init_result: i32 = glfwInit();
    if init_result == 0 {{
        print("Failed to initialize GLFW!\\n");
        return;
    }}

    print("GLFW initialized.\\n");
    heidic_glfw_vulkan_hints();
    
    print("Creating window (800x600)...\\n");
    let window: GLFWwindow = glfwCreateWindow(800, 600, "{project_name}", 0, 0);
    if window == 0 {{
        print("Failed to create window!\\n");
        glfwTerminate();
        return;
    }}

    print("Window created.\\n");
    print("Initializing Vulkan renderer...\\n");

    let renderer_init: i32 = heidic_init_renderer(window);
    if renderer_init == 0 {{
        print("Failed to initialize renderer!\\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return;
    }}

    print("Renderer initialized!\\n");
    print("Starting render loop...\\n");
    print("Press ESC or close the window to exit.\\n");
    print("\\n");
    print("=== HOT-RELOAD AVAILABLE ===\\n");
    print("Edit values in the @hot system and use the Hotload button!\\n");
    print("\\n");

    while glfwWindowShouldClose(window) == 0 {{
        glfwPollEvents();

        if glfwGetKey(window, 256) == 1 {{ // ESC key
            glfwSetWindowShouldClose(window, 1);
        }}

        // Get rotation speed from hot-reloadable system
        let rotation_speed: f32 = get_rotation_speed();
        heidic_set_rotation_speed(rotation_speed); // pass speed to renderer for live updates
        
        heidic_render_frame(window);
        heidic_sleep_ms(16); // ~60 FPS cap
    }}

    print("Cleaning up...\\n");
    heidic_cleanup_renderer();
    glfwDestroyWindow(window);
    glfwTerminate();
    print("Program exited successfully.\\n");
    print("Done!\\n");
}}
"""
                                    with open(new_path, "w", encoding="utf-8") as f:
                                        f.write(template)
                                    
                                    # Copy default shader files to project directory (all projects)
                                    script_dir = os.path.dirname(os.path.abspath(__file__))
                                    project_root = os.path.abspath(os.path.join(script_dir, ".."))
                                    shader_source_dir = os.path.join(project_root, "examples", "spinning_triangle")
                                    shaders_to_copy = ["vert_3d.spv", "frag_3d.spv"]
                                    
                                    for shader_file in shaders_to_copy:
                                        source_shader = os.path.join(shader_source_dir, shader_file)
                                        dest_shader = os.path.join(project_dir, shader_file)
                                        if os.path.exists(source_shader):
                                            try:
                                                import shutil
                                                shutil.copy2(source_shader, dest_shader)
                                                editor.log_lines.append(f"Copied shader: {shader_file}")
                                            except Exception as e:
                                                editor.log_lines.append(f"WARNING: Failed to copy {shader_file}: {e}")
                                        else:
                                            editor.log_lines.append(f"WARNING: Shader not found: {source_shader}")
                                    
                                    editor.path = new_path
                                    editor.original_hd_path = new_path
                                    editor.view_mode = "hd"
                                    editor.viewing_hd = True
                                    editor.current_shader_path = None
                                    editor.lines = editor._load_file(new_path)
                                    editor.cursor_x = 0
                                    editor.cursor_y = 0
                                    editor.scroll = 0
                                    editor.hscroll = 0
                                    editor.log_lines = []
                                    editor.log_vscroll = 0
                                    editor.log_hscroll = 0
                                    editor.status = f"Created project: {project_name} at {new_path}"
                            elif action == "load":
                                selected_path = project_picker_dialog(screen, editor)
                                if selected_path and os.path.exists(selected_path):
                                    editor.path = selected_path
                                    editor.original_hd_path = selected_path if selected_path.endswith(".hd") else selected_path
                                    editor.view_mode = "hd"
                                    editor.viewing_hd = True
                                    editor.current_shader_path = None
                                    editor.lines = editor._load_file(selected_path)
                                    editor._reset_editor_state()
                                    editor.status = f"Loaded {selected_path}"
                            elif action == "save":
                                editor.save()
                            elif action == "run":
                                editor.run_commands()
                            elif action == "hotload":
                                if editor.has_hot_systems():
                                    editor.hotload_dll()
                            elif action == "compile_shaders":
                                if editor.has_shaders():
                                    editor.compile_shaders_only()
                            elif action == "load_shader":
                                if editor.has_shaders():
                                    selected_shader = shader_picker_dialog(screen, editor)
                                    if selected_shader:
                                        editor.load_shader(selected_shader)
                            elif action == "cpp":
                                editor.toggle_view()  # Cycles HD -> C++ -> SD -> HD
                            elif action == "reload":
                                editor.lines = editor._load_file(editor.path)
                                editor.cursor_x = 0
                                editor.cursor_y = 0
                                editor.scroll = 0
                                editor.hscroll = 0
                                editor.log_lines = []
                                editor.log_vscroll = 0
                                editor.log_hscroll = 0
                                editor.status = f"Reloaded {editor.path}"
                            elif action == "quit":
                                running = False
                            break
                # Scrollbar interactions
                sb = editor.calc_scrollbar()
                if sb:
                    sb_rect = pygame.Rect(
                        offset_x + sb["scrollbar_x"],
                        sb["scrollbar_y"],
                        editor.scrollbar_width,
                        sb["usable_height"],
                    )
                    bar_rect = pygame.Rect(
                        sb_rect.x,
                        sb_rect.y + sb["bar_pos"],
                        sb_rect.w,
                        sb["bar_height"],
                    )
                    if bar_rect.collidepoint(mx, my):
                        editor.dragging_scrollbar = True
                        editor._drag_offset = my - bar_rect.y
                    elif sb_rect.collidepoint(mx, my):
                        rel = my - sb_rect.y
                        target = max(0, min(sb["usable_height"] - sb["bar_height"], rel - sb["bar_height"] // 2))
                        if (sb["usable_height"] - sb["bar_height"]) > 0:
                            editor.scroll = int((target / (sb["usable_height"] - sb["bar_height"])) * sb["max_scroll"])
                        else:
                            editor.scroll = 0
                # Horizontal scrollbar interactions
                hsb = editor.calc_hscrollbar()
                if hsb:
                    hsb_rect = pygame.Rect(
                        offset_x + hsb["track_x"],
                        hsb["track_y"],
                        hsb["usable_width"],
                        HSCROLLBAR_HEIGHT,
                    )
                    thumb_rect = pygame.Rect(
                        offset_x + hsb["track_x"] + hsb["bar_pos"],
                        hsb["track_y"],
                        hsb["bar_width"],
                        HSCROLLBAR_HEIGHT,
                    )
                    if thumb_rect.collidepoint(mx, my):
                        editor.dragging_hscroll = True
                        editor._h_drag_offset = mx - thumb_rect.x
                    elif hsb_rect.collidepoint(mx, my):
                        rel = mx - hsb_rect.x
                        target = max(0, min(hsb["usable_width"] - hsb["bar_width"], rel - hsb["bar_width"] // 2))
                        if (hsb["usable_width"] - hsb["bar_width"]) > 0:
                            editor.hscroll = int((target / (hsb["usable_width"] - hsb["bar_width"])) * hsb["max_scroll"])
                        else:
                            editor.hscroll = 0
            elif event.type == pygame.MOUSEMOTION:
                mx, my = event.pos
                # Handle dragging first (if any drag is active)
                if editor.dragging_scrollbar:
                    sb = editor.calc_scrollbar()
                    if sb:
                        # Editor surface Y starts at 0 on screen, so no Y offset needed
                        bar_track_top = sb["scrollbar_y"]
                        rel = my - bar_track_top - editor._drag_offset
                        rel = max(0, min(sb["usable_height"] - sb["bar_height"], rel))
                        editor.scroll = int((rel / (sb["usable_height"] - sb["bar_height"])) * sb["max_scroll"]) if (sb["usable_height"] - sb["bar_height"]) > 0 else 0
                elif editor.dragging_hscroll:
                    hsb = editor.calc_hscrollbar()
                    if hsb:
                        offset_x = SCREEN_WIDTH - EDITOR_WIDTH
                        rel = mx - (offset_x + hsb["track_x"]) - editor._h_drag_offset
                        rel = max(0, min(hsb["usable_width"] - hsb["bar_width"], rel))
                        editor.hscroll = int((rel / (hsb["usable_width"] - hsb["bar_width"])) * hsb["max_scroll"]) if (hsb["usable_width"] - hsb["bar_width"]) > 0 else 0
                elif editor.dragging_log_v:
                    panel_width = SCREEN_WIDTH - EDITOR_WIDTH
                    log_height = SCREEN_HEIGHT // 2
                    v_sb = editor.calc_log_vscrollbar(panel_width, log_height)
                    if v_sb:
                        rel = my - v_sb["scrollbar_y"] - editor._log_drag_offset
                        rel = max(0, min(v_sb["usable_height"] - v_sb["bar_height"], rel))
                        editor.log_vscroll = int((rel / (v_sb["usable_height"] - v_sb["bar_height"])) * v_sb["max_scroll"]) if (v_sb["usable_height"] - v_sb["bar_height"]) > 0 else 0
                elif editor.dragging_log_h:
                    panel_width = SCREEN_WIDTH - EDITOR_WIDTH
                    log_height = SCREEN_HEIGHT // 2
                    h_sb = editor.calc_log_hscrollbar(panel_width, log_height, 0)
                    if h_sb:
                        rel = mx - h_sb["track_x"] - editor._log_h_drag_offset
                        rel = max(0, min(h_sb["usable_width"] - h_sb["bar_width"], rel))
                        editor.log_hscroll = int((rel / (h_sb["usable_width"] - h_sb["bar_width"])) * h_sb["max_scroll"]) if (h_sb["usable_width"] - h_sb["bar_width"]) > 0 else 0
                elif editor.dragging_terminal_v:
                    panel_width = SCREEN_WIDTH - EDITOR_WIDTH
                    terminal_height = SCREEN_HEIGHT - (SCREEN_HEIGHT // 2)
                    v_sb_term = editor.calc_terminal_vscrollbar(panel_width, terminal_height)
                    if v_sb_term:
                        terminal_y = SCREEN_HEIGHT // 2
                        scrollbar_y_pos = terminal_y + TERMINAL_HEADER_HEIGHT + MARGIN
                        rel = my - scrollbar_y_pos - editor._terminal_drag_offset
                        rel = max(0, min(v_sb_term["usable_height"] - v_sb_term["bar_height"], rel))
                        editor.terminal_vscroll = int((rel / (v_sb_term["usable_height"] - v_sb_term["bar_height"])) * v_sb_term["max_scroll"]) if (v_sb_term["usable_height"] - v_sb_term["bar_height"]) > 0 else 0
                elif editor.dragging_terminal_h:
                    panel_width = SCREEN_WIDTH - EDITOR_WIDTH
                    terminal_height = SCREEN_HEIGHT - (SCREEN_HEIGHT // 2)
                    terminal_y = SCREEN_HEIGHT // 2
                    h_sb_term = editor.calc_terminal_hscrollbar(panel_width, terminal_height, terminal_y)
                    if h_sb_term:
                        rel = mx - h_sb_term["track_x"] - editor._terminal_h_drag_offset
                        rel = max(0, min(h_sb_term["usable_width"] - h_sb_term["bar_width"], rel))
                        editor.terminal_hscroll = int((rel / (h_sb_term["usable_width"] - h_sb_term["bar_width"])) * h_sb_term["max_scroll"]) if (h_sb_term["usable_width"] - h_sb_term["bar_width"]) > 0 else 0
                else:
                    # Check for menu button hover (for tooltips) only when not dragging
                    offset_x = SCREEN_WIDTH - EDITOR_WIDTH
                    local_x = mx - offset_x
                    local_y = my
                    editor.menu_hovered_action = None
                    if 0 <= local_y < MENU_HEIGHT and 0 <= local_x < EDITOR_WIDTH:
                        for rect, icon_char, tooltip, action in editor.menu_hitboxes:
                            if rect.collidepoint(local_x, local_y):
                                editor.menu_hovered_action = action
                                break
            elif event.type == pygame.KEYDOWN:
                # Editor input handling (terminal is read-only)
                if event.key == pygame.K_ESCAPE:
                    running = False
                elif event.key == pygame.K_F5:
                    editor.run_commands()
                elif event.key in (pygame.K_UP, pygame.K_DOWN, pygame.K_LEFT, pygame.K_RIGHT):
                    dy = -1 if event.key == pygame.K_UP else 1 if event.key == pygame.K_DOWN else 0
                    dx = -1 if event.key == pygame.K_LEFT else 1 if event.key == pygame.K_RIGHT else 0
                    editor.move_cursor(dx, dy)
                elif event.key == pygame.K_PAGEUP:
                    editor.scroll_view(-SCROLL_STEP)
                elif event.key == pygame.K_PAGEDOWN:
                    editor.scroll_view(SCROLL_STEP)
                else:
                    editor.handle_key(event)
            elif event.type == pygame.MOUSEBUTTONUP and event.button == 1:
                editor.dragging_scrollbar = False
                editor.dragging_hscroll = False
                editor.dragging_log_v = False
                editor.dragging_log_h = False
                editor.dragging_terminal_v = False
                editor.dragging_terminal_h = False
            elif event.type == pygame.MOUSEWHEEL:
                # Disable wheel scrolling globally
                pass

        editor.draw(screen)
        pygame.display.flip()
        clock.tick(60)

    editor.cleanup()
    pygame.quit()


if __name__ == "__main__":
    main()

