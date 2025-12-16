#!/bin/bash

echo "Setting up HEIDIC Documentation with MkDocs..."
echo

# Check if Python is installed
if ! command -v python3 &> /dev/null; then
    echo "ERROR: Python 3 is not installed"
    echo "Please install Python 3.7+ from https://www.python.org/"
    exit 1
fi

echo "Python found!"
echo

# Install dependencies
echo "Installing MkDocs and dependencies..."
python3 -m pip install --upgrade pip
pip3 install -r requirements-docs.txt

if [ $? -ne 0 ]; then
    echo "ERROR: Failed to install dependencies"
    exit 1
fi

echo
echo "Setup complete!"
echo
echo "To start the documentation server, run from the project root:"
echo "  mkdocs serve -f mkdocs/mkdocs.yml"
echo
echo "Or from this directory:"
echo "  cd .."
echo "  mkdocs serve -f mkdocs/mkdocs.yml"
echo
echo "Then open http://127.0.0.1:8000 in your browser"
echo


