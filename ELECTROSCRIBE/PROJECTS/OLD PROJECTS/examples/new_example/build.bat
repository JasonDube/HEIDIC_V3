@echo off
setlocal

REM Change to script directory
cd /d "%~dp0"

echo Building EDEN ENGINE New Example...
echo.

REM Check if VULKAN_SDK is set
if not defined VULKAN_SDK (
    echo ERROR: VULKAN_SDK environment variable is not set!
    echo Please install the Vulkan SDK and set VULKAN_SDK to the SDK path.
    pause
    exit /b 1
)

echo Using Vulkan SDK: %VULKAN_SDK%
echo.

REM Compile the HEIDIC source first
echo Step 1: Compiling HEIDIC source...
cd ..\..
cargo run -- compile examples/new_example/new_example.hd
if errorlevel 1 (
    echo ERROR: Failed to compile HEIDIC source!
    pause
    exit /b 1
)
cd examples\new_example

echo.
echo Step 2: Compiling C++ code...

REM Set GLFW path (adjust if different)
set GLFW_PATH=C:\glfw-3.4
if not exist "%GLFW_PATH%\include" (
    echo WARNING: GLFW not found at %GLFW_PATH%\include
    echo Please set GLFW_PATH environment variable or update build.bat
)

REM Compile with all necessary include paths
REM -I../.. adds project root so "stdlib/vulkan.h" can be found
REM -I"%GLFW_PATH%\include" adds GLFW headers
REM -I"%VULKAN_SDK%\Include" adds Vulkan headers
g++ -std=c++17 -O3 ^
    -I../.. ^
    -I"%GLFW_PATH%\include" ^
    -I"%VULKAN_SDK%\Include" ^
    new_example.cpp ^
    ..\..\vulkan\eden_vulkan_helpers.cpp ^
    -o new_example.exe ^
    -L"%VULKAN_SDK%\Lib" ^
    -L"%GLFW_PATH%\build\src" ^
    -lvulkan-1 ^
    -lglfw3 ^
    -lgdi32

if errorlevel 1 (
    echo ERROR: Failed to compile C++ code!
    echo.
    echo Make sure:
    echo - GLFW3 is installed and in your library path
    echo - GLM is installed (or add -I flag for GLM path)
    pause
    exit /b 1
)

echo.
echo Build successful! Run new_example.exe to start the application.
pause

