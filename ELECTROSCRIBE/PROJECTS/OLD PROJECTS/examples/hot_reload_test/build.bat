@echo off
REM Build script for hot-reload test example
REM This script compiles the HEIDIC source, generates DLL, and compiles the executable

echo ========================================
echo Building Hot-Reload Test Example
echo ========================================
echo.

REM Get the directory where this script is located
set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%"

REM Step 1: Compile HEIDIC source file
echo [1/4] Compiling HEIDIC source file...
cd ..\..
cargo run -- compile examples/hot_reload_test/hot_reload_test.hd
if errorlevel 1 (
    echo ERROR: Failed to compile HEIDIC source file
    pause
    exit /b 1
)
echo.

REM Step 2: Compile the hot-reloadable DLL
echo [2/4] Compiling hot-reloadable DLL...
cd examples\hot_reload_test
if exist rotation_hot.dll.cpp (
    g++ -std=c++17 -shared -fPIC -o rotation.dll rotation_hot.dll.cpp
    if errorlevel 1 (
        echo ERROR: Failed to compile DLL
        pause
        exit /b 1
    )
    echo DLL compiled successfully: rotation.dll
) else (
    echo WARNING: rotation_hot.dll.cpp not found, skipping DLL compilation
)
echo.

REM Step 3: Compile the main executable
echo [3/4] Compiling main executable...
REM Get the project root directory (two levels up from examples/hot_reload_test)
set PROJECT_ROOT=%~dp0..\..

REM Check for Vulkan SDK
set VULKAN_SDK_PATH=
if defined VK_SDK_PATH (
    set VULKAN_SDK_PATH=%VK_SDK_PATH%
    echo Found Vulkan SDK via VK_SDK_PATH: %VULKAN_SDK_PATH%
) else if exist "C:\VulkanSDK" (
    REM Try to find latest Vulkan SDK version
    for /f "delims=" %%i in ('dir /b /ad /o-n "C:\VulkanSDK" 2^>nul') do (
        set VULKAN_SDK_PATH=C:\VulkanSDK\%%i
        echo Found Vulkan SDK: %VULKAN_SDK_PATH%
        goto :found_vulkan
    )
)
:found_vulkan

REM Check for GLFW
set GLFW_PATH=
if defined GLFW_PATH_ENV (
    set GLFW_PATH=%GLFW_PATH_ENV%
    echo Found GLFW via GLFW_PATH_ENV: %GLFW_PATH%
) else if exist "C:\glfw-3.4" (
    set GLFW_PATH=C:\glfw-3.4
    echo Found GLFW: %GLFW_PATH%
) else if exist "C:\glfw" (
    set GLFW_PATH=C:\glfw
    echo Found GLFW: %GLFW_PATH%
) else if exist "C:\Program Files\GLFW" (
    set GLFW_PATH=C:\Program Files\GLFW
    echo Found GLFW: %GLFW_PATH%
)

REM Build compile command with include paths using delayed expansion
setlocal enabledelayedexpansion
set COMPILE_CMD=g++ -std=c++17 -O3 -I"%PROJECT_ROOT%"
if defined VULKAN_SDK_PATH (
    set "COMPILE_CMD=!COMPILE_CMD! -I%VULKAN_SDK_PATH%\Include"
)
if defined GLFW_PATH (
    REM GLFW headers are typically in include/GLFW subdirectory
    if exist "%GLFW_PATH%\include" (
        set "COMPILE_CMD=!COMPILE_CMD! -I%GLFW_PATH%\include"
        echo Using GLFW include path: %GLFW_PATH%\include
    ) else (
        set "COMPILE_CMD=!COMPILE_CMD! -I%GLFW_PATH%"
        echo Using GLFW include path: %GLFW_PATH%
    )
    REM Also add library path for linking
    if exist "%GLFW_PATH%\lib" (
        set "COMPILE_CMD=!COMPILE_CMD! -L%GLFW_PATH%\lib"
        echo Using GLFW library path: %GLFW_PATH%\lib
    ) else if exist "%GLFW_PATH%\build\src" (
        set "COMPILE_CMD=!COMPILE_CMD! -L%GLFW_PATH%\build\src"
        echo Using GLFW library path: %GLFW_PATH%\build\src
    ) else if exist "%GLFW_PATH%\lib-mingw-w64" (
        set "COMPILE_CMD=!COMPILE_CMD! -L%GLFW_PATH%\lib-mingw-w64"
        echo Using GLFW library path: %GLFW_PATH%\lib-mingw-w64
    )
)

REM Add GLFW library and Windows system libraries to link command
REM GLFW on Windows requires: gdi32, user32, kernel32, opengl32
REM Vulkan requires: vulkan-1 (Vulkan loader)
set LINK_LIBS=-lrotation
if defined GLFW_PATH (
    set "LINK_LIBS=!LINK_LIBS! -lglfw3 -lgdi32 -luser32 -lkernel32 -lopengl32"
)
if defined VULKAN_SDK_PATH (
    REM Add Vulkan library path and link against Vulkan
    if exist "%VULKAN_SDK_PATH%\Lib" (
        set "COMPILE_CMD=!COMPILE_CMD! -L%VULKAN_SDK_PATH%\Lib"
    )
    set "LINK_LIBS=!LINK_LIBS! -lvulkan-1"
)

REM Add Vulkan helper source file to compile
set VULKAN_HELPERS=%PROJECT_ROOT%\vulkan\eden_vulkan_helpers.cpp
set COMPILE_SOURCES=hot_reload_test.cpp
if exist "%VULKAN_HELPERS%" (
    set "COMPILE_SOURCES=!COMPILE_SOURCES! %VULKAN_HELPERS%"
)

REM Try compiling with rotation library
echo Executing: !COMPILE_CMD! -o hot_reload_test.exe !COMPILE_SOURCES! -L. !LINK_LIBS!
!COMPILE_CMD! -o hot_reload_test.exe !COMPILE_SOURCES! -L. !LINK_LIBS!
if errorlevel 1 (
    echo.
    echo Trying without -lrotation flag...
    set "LINK_LIBS="
    if defined GLFW_PATH (
        set "LINK_LIBS=-lglfw3 -lgdi32 -luser32 -lkernel32 -lopengl32"
    )
    echo Executing: !COMPILE_CMD! -o hot_reload_test.exe !COMPILE_SOURCES! -L. !LINK_LIBS!
    !COMPILE_CMD! -o hot_reload_test.exe !COMPILE_SOURCES! -L. !LINK_LIBS!
    if errorlevel 1 (
        echo.
        echo ERROR: Failed to compile main executable
        echo.
        if not defined VULKAN_SDK_PATH (
            echo NOTE: Vulkan SDK not found. Please install Vulkan SDK or set VK_SDK_PATH environment variable.
            echo Download from: https://vulkan.lunarg.com/sdk/home
            echo.
        )
        if not defined GLFW_PATH (
            echo NOTE: GLFW not found. Please install GLFW or set GLFW_PATH_ENV environment variable.
            echo Download from: https://www.glfw.org/download.html
            echo.
            echo Common locations: C:\glfw-3.4 or C:\glfw
            echo.
        )
        if defined VULKAN_SDK_PATH (
            echo Vulkan SDK found at: %VULKAN_SDK_PATH%
        )
        if defined GLFW_PATH (
            echo GLFW found at: %GLFW_PATH%
        )
        endlocal
        pause
        exit /b 1
    )
)
endlocal
echo Executable compiled successfully: hot_reload_test.exe
echo.

REM Step 4: Check if all files exist
echo [4/4] Verifying build...
if exist hot_reload_test.exe (
    echo [OK] hot_reload_test.exe
) else (
    echo [FAIL] hot_reload_test.exe not found
    pause
    exit /b 1
)

if exist rotation.dll (
    echo [OK] rotation.dll
) else (
    echo [WARN] rotation.dll not found (may need to compile manually)
)

echo.
echo ========================================
echo Build Complete!
echo ========================================
echo.
echo To run: hot_reload_test.exe
echo.
echo To test hot-reload:
echo   1. Run the game
echo   2. Edit rotation_speed in hot_reload_test.hd
echo   3. Save the file
echo   4. Run: rebuild_dll.bat
echo   5. The game will automatically reload!
echo.
pause
