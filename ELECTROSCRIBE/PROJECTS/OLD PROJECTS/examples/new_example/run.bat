@echo off
echo Running EDEN ENGINE New Example...
cd /d "%~dp0"
if exist new_example.exe (
    new_example.exe
) else (
    echo ERROR: new_example.exe not found!
    echo Please run build.bat first to compile the example.
    pause
)
pause

