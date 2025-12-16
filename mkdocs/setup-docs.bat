@echo off
echo Setting up HEIDIC Documentation with MkDocs...
echo.

REM Check if Python is installed
python --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python is not installed or not in PATH
    echo Please install Python 3.7+ from https://www.python.org/
    pause
    exit /b 1
)

echo Python found!
echo.

REM Install dependencies
echo Installing MkDocs and dependencies...
python -m pip install --upgrade pip
pip install -r requirements-docs.txt

if errorlevel 1 (
    echo ERROR: Failed to install dependencies
    pause
    exit /b 1
)

echo.
echo Setup complete!
echo.
echo To start the documentation server, run from the project root:
echo   mkdocs serve -f mkdocs/mkdocs.yml
echo.
echo Or from this directory:
echo   cd ..
echo   mkdocs serve -f mkdocs/mkdocs.yml
echo.
echo Then open http://127.0.0.1:8000 in your browser
echo.
pause


