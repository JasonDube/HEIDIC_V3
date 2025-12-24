@echo off
echo ============================================
echo   SLAG LEGION - Modular Build
echo ============================================
echo.

cd /d "%~dp0"

REM Step 1: Compile HEIDIC source to C++
echo [1/3] Compiling slag_legion.hd to C++...
heidic_v2 compile slag_legion.hd --no-build

if %errorlevel% neq 0 (
    echo HEIDIC compilation failed!
    pause
    exit /b 1
)

REM Step 2: Build with CMake
echo.
echo [2/3] Building with CMake...

if not exist build mkdir build
cd build

cmake .. -G "MinGW Makefiles"
if %errorlevel% neq 0 (
    echo CMake configuration failed!
    cd ..
    pause
    exit /b 1
)

cmake --build .
if %errorlevel% neq 0 (
    echo Build failed!
    cd ..
    pause
    exit /b 1
)

cd ..

REM Step 3: Copy executable
echo.
echo [3/3] Copying executable...
copy /Y build\slag_legion.exe . >nul 2>&1

echo.
echo ============================================
echo   BUILD SUCCESSFUL
echo ============================================
echo.
echo Run: slag_legion.exe
echo.
pause

