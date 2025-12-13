@echo off
REM Quick script to rebuild just the DLL for hot-reload testing
REM Use this when you edit the .hd file and want to reload
REM This will recompile the HEIDIC source and then rebuild the DLL

REM Get the directory where this script is located
set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%"

echo ========================================
echo Rebuilding Hot-Reload DLL
echo ========================================
echo.

REM Step 1: Recompile HEIDIC source to regenerate DLL source
echo [1/2] Recompiling HEIDIC source...
cd ..\..
cargo run -- compile examples/hot_reload_test/hot_reload_test.hd
if errorlevel 1 (
    echo ERROR: Failed to recompile HEIDIC source
    pause
    exit /b 1
)
echo.

REM Step 2: Recompile the DLL
echo [2/2] Recompiling DLL...
cd examples\hot_reload_test
if exist rotation_hot.dll.cpp (
    REM Compile to a temporary file first (to avoid "Permission denied" if DLL is loaded)
    echo Compiling to temporary file...
    g++ -std=c++17 -shared -fPIC -o rotation.dll.new rotation_hot.dll.cpp
    if errorlevel 1 (
        echo ERROR: Failed to compile DLL
        pause
        exit /b 1
    )
    
    REM Robust replace loop: try multiple times while the game unloads the DLL
    set RETRIES=10
    set SLEEP=1
    set REPLACED=0
    echo Attempting to replace old DLL (up to %RETRIES% retries)...
    for /l %%i in (1,1,%RETRIES%) do (
        if exist rotation.dll.new (
            if exist rotation.dll (
                del /F /Q rotation.dll 2>nul
            )
            move /Y rotation.dll.new rotation.dll >nul 2>&1
            if not exist rotation.dll.new (
                set REPLACED=1
            ) else (
                if %%i lss %RETRIES% (
                    echo DLL locked; retrying in %SLEEP%s... (attempt %%i of %RETRIES%)
                    timeout /t %SLEEP% /nobreak >nul
                )
            )
        )
    )
    if "%REPLACED%"=="0" (
        echo.
        echo ERROR: Could not replace DLL (still locked)
        echo.
        echo SOLUTION: The game needs to unload the DLL first.
        echo 1. Make sure the game is running
        echo 2. Wait a moment for the game to detect the change
        echo 3. Run rebuild_dll.bat again
        echo.
        echo Or: Close the game, run rebuild_dll.bat, then restart the game
        pause
        exit /b 1
    )
    
    echo.
    echo ========================================
    echo DLL rebuilt successfully: rotation.dll
    echo ========================================
    echo.
    echo The game should automatically detect and reload it!
    echo (Make sure the game is running in another window)
    echo.
) else (
    echo ERROR: rotation_hot.dll.cpp not found
    echo This should have been generated in step 1
    pause
    exit /b 1
)

