@echo off
echo ============================================
echo       SLAG LEGION - Build Script
echo ============================================

REM Navigate to project directory
cd /d "%~dp0"

REM Check for HEIDIC compiler
where heidic_v2 >nul 2>nul
if %errorlevel% neq 0 (
    echo ERROR: HEIDIC compiler not found in PATH
    echo Please ensure heidic_v2.exe is available
    pause
    exit /b 1
)

echo.
echo Compiling slag_legion.hd...
heidic_v2 compile slag_legion.hd

if %errorlevel% neq 0 (
    echo.
    echo BUILD FAILED
    pause
    exit /b 1
)

echo.
echo ============================================
echo       BUILD SUCCESSFUL
echo ============================================
echo.
echo Run: slag_legion.exe
echo.
pause

