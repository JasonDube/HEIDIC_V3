@echo off
echo Compiling SLAG LEGION shaders...

glslangValidator -V cube.vert -o vert_3d.spv
if %errorlevel% neq 0 (
    echo Failed to compile vertex shader!
    pause
    exit /b 1
)

glslangValidator -V cube.frag -o frag_3d.spv
if %errorlevel% neq 0 (
    echo Failed to compile fragment shader!
    pause
    exit /b 1
)

echo Shaders compiled successfully!

