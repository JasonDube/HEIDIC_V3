# HEIDIC Documentation - MkDocs Setup

This directory contains all files related to building the HEIDIC documentation website using MkDocs.

## Files

- `mkdocs.yml` - MkDocs configuration file
- `requirements-docs.txt` - Python dependencies for MkDocs
- `setup-docs.bat` - Windows setup script
- `setup-docs.sh` - Linux/Mac setup script

## Setup

### Windows
```bash
cd mkdocs
setup-docs.bat
```

### Linux/Mac
```bash
cd mkdocs
chmod +x setup-docs.sh
./setup-docs.sh
```

## Running the Documentation Server

From the project root directory:

```bash
mkdocs serve -f mkdocs/mkdocs.yml
```

Then open http://127.0.0.1:8000 in your browser.

## Building the Documentation

To build a static site:

```bash
mkdocs build -f mkdocs/mkdocs.yml
```

The built site will be in the `site/` directory (in the project root).

## Hosting the Documentation

### GitHub Pages (Recommended)

The repository includes a GitHub Actions workflow (`.github/workflows/docs.yml`) that automatically builds and deploys the documentation to GitHub Pages.

**Setup:**
1. Push the workflow file to your repository
2. Go to repository Settings → Pages
3. Select "GitHub Actions" as the source
4. The documentation will deploy automatically on every push to `main`

**URL:** `https://yourusername.github.io/heidic/`

### Netlify

1. Sign up at [netlify.com](https://netlify.com)
2. Connect your GitHub repository
3. Build settings:
   - Build command: `cd mkdocs && pip install -r requirements-docs.txt && mkdocs build -f mkdocs.yml`
   - Publish directory: `site`
4. Deploy!

**Configuration file:** `mkdocs/netlify.toml` (already included)

### Vercel

1. Sign up at [vercel.com](https://vercel.com)
2. Import your GitHub repository
3. Vercel will auto-detect the configuration from `mkdocs/vercel.json`
4. Deploy!

**Configuration file:** `mkdocs/vercel.json` (already included)

## Notes

- The `docs_dir` in `mkdocs.yml` points to `DOCS` (relative to project root)
- All paths in `mkdocs.yml` are relative to the project root when using `-f mkdocs/mkdocs.yml`
- The built `site/` directory is in the project root (as specified in `.gitignore`)

