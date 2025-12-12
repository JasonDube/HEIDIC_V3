@echo off
REM Build script for gateway_editor_v1 example
REM This script compiles HEIDIC to C++ and then builds the executable

setlocal enabledelayedexpansion

REM Get the project root (two levels up from this script)
set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%..\.."
cd /d "%PROJECT_ROOT%"

echo === EDEN ENGINE Build - Gateway Editor v1 ===
echo.

REM Step 1: Compile HEIDIC to C++
echo Compiling HEIDIC: examples\gateway_editor_v1\gateway_editor_v1.hd
cargo run -- compile examples\gateway_editor_v1\gateway_editor_v1.hd
if errorlevel 1 (
    echo HEIDIC compilation failed!
    exit /b 1
)

REM Check if C++ file was generated
if not exist "examples\gateway_editor_v1\gateway_editor_v1.cpp" (
    echo Error: Generated C++ file not found!
    exit /b 1
)

echo.
echo Compiling C++ and linking...
echo.

REM Change to gateway_editor_v1 directory
cd /d "%SCRIPT_DIR%"

REM Set up paths
set "VULKAN_SDK_PATH="
if defined VULKAN_SDK (
    set "VULKAN_SDK_PATH=%VULKAN_SDK%"
    echo Using VULKAN_SDK: %VULKAN_SDK_PATH%
) else if exist "C:\VulkanSDK" (
    for /f "delims=" %%i in ('dir /b /ad /o-n "C:\VulkanSDK"') do (
        set "VULKAN_SDK_PATH=C:\VulkanSDK\%%i"
        goto :vulkan_found
    )
    :vulkan_found
    if defined VULKAN_SDK_PATH (
        echo Found Vulkan SDK: %VULKAN_SDK_PATH%
    )
)

REM Find GLFW
set "GLFW_PATH="
set "GLFW_LIB_PATH="
set "GLFW_LIB_FILE="
set "GLFW_FOUND=0"

REM Check environment variable first
if defined GLFW_PATH (
    echo Using GLFW_PATH from environment: %GLFW_PATH%
) else if exist "C:\glfw-3.4" (
    set "GLFW_PATH=C:\glfw-3.4"
)

REM If GLFW_PATH is set, search for library
if defined GLFW_PATH (
    REM Try common library paths
    if exist "%GLFW_PATH%\build\src\libglfw3.a" (
        set "GLFW_LIB_PATH=%GLFW_PATH%\build\src"
        set "GLFW_LIB_FILE=%GLFW_PATH%\build\src\libglfw3.a"
        set "GLFW_FOUND=1"
    ) else if exist "%GLFW_PATH%\lib-mingw-w64\libglfw3.a" (
        set "GLFW_LIB_PATH=%GLFW_PATH%\lib-mingw-w64"
        set "GLFW_LIB_FILE=%GLFW_PATH%\lib-mingw-w64\libglfw3.a"
        set "GLFW_FOUND=1"
    ) else if exist "%GLFW_PATH%\lib-vc2022\x64\libglfw3.a" (
        set "GLFW_LIB_PATH=%GLFW_PATH%\lib-vc2022\x64"
        set "GLFW_LIB_FILE=%GLFW_PATH%\lib-vc2022\x64\libglfw3.a"
        set "GLFW_FOUND=1"
    ) else if exist "%GLFW_PATH%\lib\x64\libglfw3.a" (
        set "GLFW_LIB_PATH=%GLFW_PATH%\lib\x64"
        set "GLFW_LIB_FILE=%GLFW_PATH%\lib\x64\libglfw3.a"
        set "GLFW_FOUND=1"
    ) else if exist "%GLFW_PATH%\build\src\glfw3.a" (
        set "GLFW_LIB_PATH=%GLFW_PATH%\build\src"
        set "GLFW_LIB_FILE=%GLFW_PATH%\build\src\glfw3.a"
        set "GLFW_FOUND=1"
    )
    if !GLFW_FOUND! equ 1 (
        echo Found GLFW library at: !GLFW_LIB_FILE!
    ) else (
        echo Warning: GLFW library not found in standard locations.
        echo Searched in: %GLFW_PATH%
        echo.
        echo Searching recursively for GLFW library files...
        for /r "%GLFW_PATH%" %%f in (libglfw3.a glfw3.a libglfw3.dll.a) do (
            if exist "%%f" (
                echo Found GLFW library at: %%f
                set "GLFW_LIB_FILE=%%f"
                set "GLFW_LIB_PATH=%%~dpf"
                set "GLFW_FOUND=1"
                goto :glfw_search_done
            )
        )
        :glfw_search_done
        if !GLFW_FOUND! equ 0 (
            echo No GLFW library file found in %GLFW_PATH%
            echo.
            echo Directory contents of %GLFW_PATH%:
            dir /b "%GLFW_PATH%" 2>nul
            echo.
            echo You can set GLFW_PATH environment variable to specify GLFW location.
        ) else (
            REM Variables were set in the loop, verify they're set
            if defined GLFW_LIB_FILE (
                echo Using GLFW library: !GLFW_LIB_FILE!
            )
        )
    )
)

REM Build compiler flags
set "INCLUDES=-I%PROJECT_ROOT% -I%PROJECT_ROOT%\stdlib"
if defined GLFW_PATH (
    set "INCLUDES=%INCLUDES% -I%GLFW_PATH%\include"
)
if defined VULKAN_SDK_PATH (
    set "INCLUDES=%INCLUDES% -I%VULKAN_SDK_PATH%\Include"
    set "LIBPATHS=-L%VULKAN_SDK_PATH%\Lib"
)
if exist "%PROJECT_ROOT%\third_party\glm" (
    set "INCLUDES=%INCLUDES% -I%PROJECT_ROOT%\third_party\glm"
) else if exist "C:\glm\glm\glm.hpp" (
    set "INCLUDES=%INCLUDES% -IC:\glm"
)
if exist "%PROJECT_ROOT%\third_party\imgui\imgui.h" (
    set "INCLUDES=%INCLUDES% -I%PROJECT_ROOT%\third_party\imgui -I%PROJECT_ROOT%\third_party\imgui\backends"
)
REM GLFW library path will be added directly in link command to avoid backslash issues

REM Create object directory if it doesn't exist
if not exist "%PROJECT_ROOT%\vulkan\obj" mkdir "%PROJECT_ROOT%\vulkan\obj"

REM Compile eden_vulkan_helpers.cpp
echo Compiling eden_vulkan_helpers.cpp...
g++ -std=c++17 -O3 %INCLUDES% -c "%PROJECT_ROOT%\vulkan\eden_vulkan_helpers.cpp" -o "%PROJECT_ROOT%\vulkan\obj\eden_vulkan_helpers.o"
if errorlevel 1 (
    echo Failed to compile eden_vulkan_helpers!
    exit /b 1
)

REM Compile nfd_win.cpp (Native File Dialog)
if exist "%PROJECT_ROOT%\third_party\nfd_win.cpp" (
    echo Compiling nfd_win.cpp...
    g++ -std=c++17 -O3 %INCLUDES% -c "%PROJECT_ROOT%\third_party\nfd_win.cpp" -o "%PROJECT_ROOT%\vulkan\obj\nfd_win.o"
    if errorlevel 1 (
        echo Failed to compile nfd_win.cpp!
        exit /b 1
    )
)

REM Compile ImGui files if they exist
if exist "%PROJECT_ROOT%\third_party\imgui\imgui.h" (
    echo Compiling ImGui files...
    g++ -std=c++17 -O3 %INCLUDES% -c "%PROJECT_ROOT%\third_party\imgui\imgui.cpp" -o "%PROJECT_ROOT%\vulkan\obj\imgui.o"
    g++ -std=c++17 -O3 %INCLUDES% -c "%PROJECT_ROOT%\third_party\imgui\imgui_draw.cpp" -o "%PROJECT_ROOT%\vulkan\obj\imgui_draw.o"
    g++ -std=c++17 -O3 %INCLUDES% -c "%PROJECT_ROOT%\third_party\imgui\imgui_tables.cpp" -o "%PROJECT_ROOT%\vulkan\obj\imgui_tables.o"
    g++ -std=c++17 -O3 %INCLUDES% -c "%PROJECT_ROOT%\third_party\imgui\imgui_widgets.cpp" -o "%PROJECT_ROOT%\vulkan\obj\imgui_widgets.o"
    g++ -std=c++17 -O3 %INCLUDES% -c "%PROJECT_ROOT%\third_party\imgui\backends\imgui_impl_glfw.cpp" -o "%PROJECT_ROOT%\vulkan\obj\imgui_impl_glfw.o"
    g++ -std=c++17 -O3 %INCLUDES% -c "%PROJECT_ROOT%\third_party\imgui\backends\imgui_impl_vulkan.cpp" -o "%PROJECT_ROOT%\vulkan\obj\imgui_impl_vulkan.o"
)

REM Compile main source file
echo Compiling gateway_editor_v1.cpp...
g++ -std=c++17 -O3 %INCLUDES% -c gateway_editor_v1.cpp -o "%PROJECT_ROOT%\vulkan\obj\gateway_editor_v1.o"
if errorlevel 1 (
    echo Failed to compile gateway_editor_v1.cpp!
    exit /b 1
)

REM Link everything
echo Linking executable...
echo [DEBUG] PROJECT_ROOT=%PROJECT_ROOT%
echo [DEBUG] Setting OBJ_FILES...
set "OBJ_FILES=%PROJECT_ROOT%\vulkan\obj\gateway_editor_v1.o %PROJECT_ROOT%\vulkan\obj\eden_vulkan_helpers.o"
if exist "%PROJECT_ROOT%\vulkan\obj\nfd_win.o" (
    set "OBJ_FILES=%OBJ_FILES% %PROJECT_ROOT%\vulkan\obj\nfd_win.o"
)
echo [DEBUG] OBJ_FILES=%OBJ_FILES%
echo [DEBUG] Checking for ImGui objects...
if exist "%PROJECT_ROOT%\vulkan\obj\imgui.o" (
    echo [DEBUG] ImGui objects found, adding to OBJ_FILES...
    set "OBJ_FILES=%OBJ_FILES% %PROJECT_ROOT%\vulkan\obj\imgui.o %PROJECT_ROOT%\vulkan\obj\imgui_draw.o %PROJECT_ROOT%\vulkan\obj\imgui_tables.o %PROJECT_ROOT%\vulkan\obj\imgui_widgets.o %PROJECT_ROOT%\vulkan\obj\imgui_impl_glfw.o %PROJECT_ROOT%\vulkan\obj\imgui_impl_vulkan.o"
    echo [DEBUG] OBJ_FILES after ImGui=%OBJ_FILES%
) else (
    echo [DEBUG] No ImGui objects found
)

REM Prepare library flags (GLFW handled separately in link command)
echo [DEBUG] Preparing library flags...
set "LIBS="
echo [DEBUG] GLFW_FOUND=!GLFW_FOUND!

REM Check if GLFW was found - use goto to avoid problematic if/else parsing
if "!GLFW_FOUND!"=="0" goto :glfw_not_found
goto :glfw_found

:glfw_not_found
echo [DEBUG] GLFW not found, setting up fallback...
REM Only set LIBS if GLFW not found - will try pkg-config or -lglfw3
where pkg-config >nul 2>&1
if !ERRORLEVEL!==0 (
    echo Using pkg-config for GLFW...
    for /f "tokens=*" %%i in ('pkg-config --libs glfw3 2^>nul') do set "LIBS=%%i"
    if defined LIBS (
        set "LIBS=!LIBS! -lgdi32 -luser32 -lshell32"
    ) else (
        REM Fall back to trying -lglfw3 (might be in system library path)
        echo Warning: GLFW library not found in standard locations.
        echo Attempting to link with -lglfw3 (may fail if GLFW not installed)...
        set "LIBS=-lglfw3 -lgdi32 -luser32 -lshell32 -lole32 -luuid"
    )
) else (
    REM No pkg-config, try -lglfw3 anyway (might work if in system path)
    echo Warning: GLFW library not found. Attempting to link with -lglfw3...
    echo If linking fails, install GLFW or set GLFW_PATH environment variable.
    set "LIBS=-lglfw3 -lgdi32 -luser32 -lshell32"
)
goto :glfw_check_done

:glfw_found
echo [DEBUG] GLFW found, skipping fallback library setup
goto :glfw_check_done

:glfw_check_done

echo [DEBUG] Finished library flag preparation
echo [DEBUG] LIBS=!LIBS!

REM Link everything - call g++ directly to avoid batch parsing issues with paths
set "VULKAN_LIB="
if defined VULKAN_SDK_PATH (
    set "VULKAN_LIB=-lvulkan-1"
)

REM Link - build the link command directly without endlocal
echo.
echo [DEBUG] === LINKING DEBUG INFO ===
echo [DEBUG] Variables:
echo [DEBUG]   GLFW_FOUND=!GLFW_FOUND!
echo [DEBUG]   GLFW_LIB_PATH=!GLFW_LIB_PATH!
echo [DEBUG]   VULKAN_SDK_PATH=!VULKAN_SDK_PATH!
echo [DEBUG]   INCLUDES=!INCLUDES!
echo [DEBUG]   LIBPATHS=!LIBPATHS!
echo [DEBUG]   OBJ_FILES=!OBJ_FILES!
echo.

echo [DEBUG] Building link command...
set "LINK_CMD=g++ -std=c++17 -O3 !INCLUDES! !LIBPATHS! !OBJ_FILES!"
echo [DEBUG] Base command: !LINK_CMD!

if defined VULKAN_SDK_PATH (
    echo [DEBUG] Adding Vulkan library...
    set "LINK_CMD=!LINK_CMD! -lvulkan-1"
) else (
    echo [DEBUG] No Vulkan SDK found
)

if "!GLFW_FOUND!"=="1" (
    echo [DEBUG] GLFW found - processing path...
    REM Convert GLFW path to forward slashes for g++
    set "GLFW_LIB_PATH_FORWARD=!GLFW_LIB_PATH:\=/!"
    echo [DEBUG] GLFW_LIB_PATH_FORWARD=!GLFW_LIB_PATH_FORWARD!
    set "LINK_CMD=!LINK_CMD! -L!GLFW_LIB_PATH_FORWARD! -lglfw3 -lgdi32 -luser32 -lshell32 -lole32 -luuid"
) else (
    echo [DEBUG] GLFW not found - using fallback: !LIBS!
    if defined LIBS (
        set "LINK_CMD=!LINK_CMD! !LIBS!"
    )
)

set "LINK_CMD=!LINK_CMD! -o gateway_editor_v1.exe"

echo.
echo [DEBUG] === FINAL LINK COMMAND ===
echo !LINK_CMD!
echo [DEBUG] === END DEBUG INFO ===
echo.

REM Execute using delayed expansion - this avoids parsing errors with special chars
echo Executing link command...
!LINK_CMD!
set "LINK_RESULT=!ERRORLEVEL!"
echo [DEBUG] Link exit code: !LINK_RESULT!
if !LINK_RESULT! neq 0 (
    echo Linking failed!
    exit /b 1
)

echo.
echo === Build successful! ===
echo Run with: .\gateway_editor_v1.exe
echo.

endlocal

