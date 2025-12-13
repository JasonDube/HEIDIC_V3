@echo off
setlocal

REM Generic build script for EDEN ENGINE examples
REM Usage: build_example.bat <example_name> [--imgui]
REM Example: build_example.bat spinning_cube --imgui

if "%~1"=="" (
    echo Usage: build_example.bat ^<example_name^> [--imgui]
    echo Example: build_example.bat spinning_cube --imgui
    exit /b 1
)

set EXAMPLE_NAME=%~1
set USE_IMGUI=0

REM Check for --imgui flag
if "%~2"=="--imgui" set USE_IMGUI=1
if "%~3"=="--imgui" set USE_IMGUI=1

REM Change to script directory
cd /d "%~dp0"

REM Get project root (one level up from examples/)
cd ..
set PROJECT_ROOT=%CD%

echo Building %EXAMPLE_NAME% example...
echo Project root: %PROJECT_ROOT%
if %USE_IMGUI%==1 (
    echo ImGui support: ENABLED
) else (
    echo ImGui support: DISABLED (use --imgui flag to enable)
)
echo.

REM Compile HEIDIC to C++
echo Step 1: Compiling HEIDIC code...
cargo run -- compile examples/%EXAMPLE_NAME%/%EXAMPLE_NAME%.hd
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
cd examples\%EXAMPLE_NAME%

REM Compile shaders (if glslc is available and shader files exist)
echo.
echo Step 2: Compiling shaders...
if exist "vert_*.glsl" (
    if exist "%VULKAN_SDK_PATH%\Bin\glslc.exe" (
        for %%f in (vert_*.glsl) do (
            echo   Compiling %%f...
            "%VULKAN_SDK_PATH%\Bin\glslc.exe" -fshader-stage=vertex "%%f" -o "%%~nf.spv"
            if errorlevel 1 (
                echo WARNING: Failed to compile %%f
            )
        )
    )
    if exist "frag_*.glsl" (
        for %%f in (frag_*.glsl) do (
            echo   Compiling %%f...
            "%VULKAN_SDK_PATH%\Bin\glslc.exe" -fshader-stage=fragment "%%f" -o "%%~nf.spv"
            if errorlevel 1 (
                echo WARNING: Failed to compile %%f
            )
        )
    )
) else (
    echo   No shader files found, skipping...
)

REM Check for GLFW
set GLFW_PATH=C:\glfw-3.4
if not exist "%GLFW_PATH%\include" (
    echo WARNING: GLFW not found at %GLFW_PATH%\include
    echo Please set GLFW_PATH environment variable or update build script
)

REM Check for ImGui
set IMGUI_PATH=%PROJECT_ROOT%\vulkan_reference\official_samples\third_party\imgui
if %USE_IMGUI%==1 (
    if not exist "%IMGUI_PATH%\imgui.h" (
        echo WARNING: ImGui not found at %IMGUI_PATH%
        echo ImGui support will be disabled
        set USE_IMGUI=0
    )
)

REM Build compile command
echo.
echo Step 3: Compiling C++ code...

set COMPILE_CMD=g++ -std=c++17 -O3 %EXAMPLE_NAME%.cpp "%PROJECT_ROOT%\vulkan\eden_vulkan_helpers.cpp"

REM Add ImGui source files if enabled
if %USE_IMGUI%==1 (
    set COMPILE_CMD=%COMPILE_CMD% "%IMGUI_PATH%\imgui.cpp" "%IMGUI_PATH%\imgui_draw.cpp" "%IMGUI_PATH%\imgui_tables.cpp" "%IMGUI_PATH%\imgui_widgets.cpp" "%IMGUI_PATH%\backends\imgui_impl_glfw.cpp" "%IMGUI_PATH%\backends\imgui_impl_vulkan.cpp"
)

set COMPILE_CMD=%COMPILE_CMD% -o %EXAMPLE_NAME%.exe

REM Add include paths
set COMPILE_CMD=%COMPILE_CMD% -I"../.." -I"%VULKAN_SDK_PATH%\Include" -I"%GLFW_PATH%\include"

REM Add ImGui include path if enabled
if %USE_IMGUI%==1 (
    set COMPILE_CMD=%COMPILE_CMD% -I"%IMGUI_PATH%" -I"%IMGUI_PATH%\backends" -DUSE_IMGUI
)

REM Add library paths
set COMPILE_CMD=%COMPILE_CMD% -L"%VULKAN_SDK_PATH%\Lib" -L"%GLFW_PATH%\build\src"

REM Add libraries
set COMPILE_CMD=%COMPILE_CMD% -lvulkan-1 -lglfw3 -lgdi32

REM Execute compile command
%COMPILE_CMD%

if errorlevel 1 (
    echo Failed to compile C++ code!
    pause
    exit /b 1
)

echo.
echo Build successful! Run with: %EXAMPLE_NAME%.exe
if %USE_IMGUI%==1 (
    echo ImGui is enabled - you can use heidic_init_imgui() in your code
)
pause

