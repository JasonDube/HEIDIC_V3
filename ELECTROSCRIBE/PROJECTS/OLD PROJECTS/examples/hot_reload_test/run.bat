@echo off
REM Run the hot-reload test example

echo Running Hot-Reload Test...
echo.

if not exist hot_reload_test.exe (
    echo ERROR: hot_reload_test.exe not found
    echo Run build.bat first to compile
    pause
    exit /b 1
)

if not exist rotation.dll (
    echo WARNING: rotation.dll not found
    echo The game may not work correctly
    echo.
)

echo Starting game...
echo.
echo ========================================
echo HOT-RELOAD INSTRUCTIONS:
echo ========================================
echo 1. Edit rotation_speed in hot_reload_test.hd
echo 2. Save the file
echo 3. Run rebuild_dll.bat (or compile manually)
echo 4. Watch the triangle spin faster/slower!
echo.
echo Press ESC or close window to exit
echo ========================================
echo.

hot_reload_test.exe

