# HEIDIC Documentation

This directory contains the source files for the HEIDIC documentation website.

## Building the Documentation

### Prerequisites

Install Python dependencies:

```bash
pip install -r requirements-docs.txt
```

### Local Development

Start the development server:

```bash
mkdocs serve
```

The documentation will be available at `http://127.0.0.1:8000`

### Building for Production

Build static HTML files:

```bash
mkdocs build
```

The output will be in the `site/` directory.

### Deploying

Deploy to GitHub Pages:

```bash
mkdocs gh-deploy
```

Or deploy to any static hosting service by uploading the `site/` directory.

## Documentation Structure

The documentation is organized in `mkdocs.yml`. Source files are in the `DOCS/` directory and are referenced by their paths in the navigation.

## Adding New Documentation

1. Add your markdown file to the appropriate directory in `DOCS/`
2. Add an entry to the `nav` section in `mkdocs.yml`
3. The file will automatically be included in the build

