# Deploying HEIDIC Documentation to GitHub Pages

## Quick Setup (5 minutes)

### Step 1: Enable GitHub Pages

1. Go to your GitHub repository
2. Click **Settings** (top menu)
3. Scroll down to **Pages** (left sidebar)
4. Under **Source**, select **"GitHub Actions"**
5. Click **Save**

### Step 2: Push Your Code

The GitHub Actions workflow (`.github/workflows/docs.yml`) is already configured. Just push to `main`:

```bash
git push origin main
```

### Step 3: Check Deployment

1. Go to the **Actions** tab in your repository
2. You should see a workflow run called "Deploy Documentation"
3. Wait for it to complete (green checkmark)
4. Your docs will be live at: `https://yourusername.github.io/heidic/`

## Your Documentation URL

After deployment, your documentation will be available at:

**`https://yourusername.github.io/heidic/`**

Replace `yourusername` with your actual GitHub username and `heidic` with your repository name.

## Adding a Documentation Badge

Add this to your README.md to link to the docs:

```markdown
[![Documentation](https://img.shields.io/badge/docs-online-blue)](https://yourusername.github.io/heidic/)
```

## Troubleshooting

### Docs not deploying?

1. **Check Actions tab** - Look for errors in the workflow run
2. **Verify Pages is enabled** - Settings → Pages → Source should be "GitHub Actions"
3. **Check branch** - Make sure you're pushing to `main` branch
4. **Wait a few minutes** - First deployment can take 2-3 minutes

### Docs not updating?

- The workflow only runs when files in `public-docs/`, `mkdocs/`, or `.github/workflows/docs.yml` change
- Make sure you're editing files in `public-docs/` (not `DOCS/`)
- Check the Actions tab to see if the workflow ran

### Custom Domain

To use a custom domain (e.g., `docs.heidic.dev`):

1. Add a `CNAME` file in `public-docs/` with your domain
2. Configure DNS records for your domain
3. Update `site_url` in `mkdocs/mkdocs.yml`

## Manual Deployment

If you want to build and deploy manually:

```bash
# Install dependencies
cd mkdocs
pip install -r requirements-docs.txt

# Build the site
cd ..
mkdocs build -f mkdocs/mkdocs.yml

# The built site is in the `site/` folder
# You can deploy this folder to any static hosting service
```

