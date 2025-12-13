@echo off
REM This build script now uses the generic build_example.bat
REM You can also use: ..\build_example.bat spinning_cube [--imgui]

call ..\build_example.bat spinning_cube %*


REM Compile HEIDIC to C++
echo Compiling HEIDIC code...
cargo run -- compile examples/spinning_cube/spinning_cube.hd
if errorlevel 1 (
    echo Failed to compile HEIDIC code!
    pause
    exit /b 1
)

REM Check for Vulkan SDK
if not defined VULKAN_SDK (
    REM Try to find it automatically
    if exist "C:\VulkanSDK" (
        for /d %%d in ("C:\VulkanSDK\*") do (
            set VULKAN_SDK=%%d
        )
    )
)

if not defined VULKAN_SDK (
    echo ERROR: VULKAN_SDK environment variable is not set!
    echo Please install the Vulkan SDK and set VULKAN_SDK to the SDK path.
    pause
    exit /b 1
)

set VULKAN_SDK_PATH=%VULKAN_SDK%
echo Using Vulkan SDK: %VULKAN_SDK_PATH%

REM Go back to example directory
cd examples\spinning_cube

REM Compile shaders (if glslc is available)
echo Compiling shaders...
if exist "%VULKAN_SDK_PATH%\Bin\glslc.exe" (
    "%VULKAN_SDK_PATH%\Bin\glslc.exe" -fshader-stage=vertex vert_cube.glsl -o vert_cube.spv
    if errorlevel 1 (
        echo WARNING: Failed to compile vertex shader
    )
    "%VULKAN_SDK_PATH%\Bin\glslc.exe" -fshader-stage=fragment frag_cube.glsl -o frag_cube.spv
    if errorlevel 1 (
        echo WARNING: Failed to compile fragment shader
    )
) else (
    echo WARNING: glslc not found at %VULKAN_SDK_PATH%\Bin\glslc.exe
    echo Please compile shaders manually:
    echo   glslc -fshader-stage=vertex vert_cube.glsl -o vert_cube.spv
    echo   glslc -fshader-stage=fragment frag_cube.glsl -o frag_cube.spv
)

REM Check for GLFW
set GLFW_PATH=C:\glfw-3.4
if not exist "%GLFW_PATH%\include" (
    echo WARNING: GLFW not found at %GLFW_PATH%\include
    echo Please set GLFW_PATH environment variable or update build.bat
)

REM Compile C++ code
echo Compiling C++ code...
g++ -std=c++17 -O3 spinning_cube.cpp "%PROJECT_ROOT%\vulkan\eden_vulkan_helpers.cpp" -o spinning_cube.exe ^
    -I"../.." ^
    -I"%VULKAN_SDK_PATH%\Include" ^
    -I"%GLFW_PATH%\include" ^
    -L"%VULKAN_SDK_PATH%\Lib" ^
    -L"%GLFW_PATH%\build\src" ^
    -lvulkan-1 -lglfw3 -lgdi32

if errorlevel 1 (
    echo Failed to compile C++ code!
    pause
    exit /b 1
)

echo.
echo Build successful! Run with: spinning_cube.exe
pause

