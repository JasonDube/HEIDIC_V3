# Images Directory

Place your images here for use in the documentation.

## How to Add Images

### 1. Add your image file
Place your image file (`.png`, `.jpg`, `.gif`, `.svg`, etc.) in this directory or a subdirectory.

### 2. Reference in Markdown
Use standard markdown image syntax:

```markdown
![Alt text](images/your-image.png)
```

### 3. Examples

**Basic image:**
```markdown
![HEIDIC Logo](images/logo.png)
```

**Image with caption (using HTML):**
```html
<figure>
  <img src="images/architecture-diagram.png" alt="HEIDIC Architecture">
  <figcaption>HEIDIC System Architecture</figcaption>
</figure>
```

**Image with link:**
```markdown
[![HEIDIC Logo](images/logo.png)](https://github.com/JasonDube/HEIDIC)
```

**Relative paths from subdirectories:**
If your markdown file is in `HEIDIC/`, use:
```markdown
![Image](../images/your-image.png)
```

## Supported Formats

- PNG (`.png`)
- JPEG (`.jpg`, `.jpeg`)
- GIF (`.gif`)
- SVG (`.svg`)
- WebP (`.webp`)

## Tips

- Use descriptive filenames (e.g., `heidic-architecture-diagram.png` instead of `img1.png`)
- Optimize images for web (keep file sizes reasonable)
- Use SVG for diagrams when possible (scales better)
- Consider creating subdirectories for organization (e.g., `images/diagrams/`, `images/screenshots/`)

---

## Embedding YouTube Videos

MkDocs supports embedding YouTube videos using HTML iframes. The Material theme will automatically make them responsive.

### Basic YouTube Embed

```html
<iframe width="560" height="315" src="https://www.youtube.com/embed/VIDEO_ID" 
        frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture" 
        allowfullscreen></iframe>
```

Replace `VIDEO_ID` with the actual YouTube video ID (the part after `v=` in the URL).

**Example:**
If your YouTube URL is: `https://www.youtube.com/watch?v=dQw4w9WgXcQ`
Then `VIDEO_ID` is: `dQw4w9WgXcQ`

### Responsive YouTube Embed (Recommended)

For better mobile support, wrap it in a div:

```html
<div style="position: relative; padding-bottom: 56.25%; height: 0; overflow: hidden; max-width: 100%;">
  <iframe style="position: absolute; top: 0; left: 0; width: 100%; height: 100%;" 
          src="https://www.youtube.com/embed/VIDEO_ID" 
          frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture" 
          allowfullscreen></iframe>
</div>
```

### With Caption

```html
<figure>
  <div style="position: relative; padding-bottom: 56.25%; height: 0; overflow: hidden; max-width: 100%;">
    <iframe style="position: absolute; top: 0; left: 0; width: 100%; height: 100%;" 
            src="https://www.youtube.com/embed/VIDEO_ID" 
            frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture" 
            allowfullscreen></iframe>
  </div>
  <figcaption>HEIDIC Tutorial: Getting Started</figcaption>
</figure>
```

### Quick Reference

- **Get Video ID:** From URL `https://www.youtube.com/watch?v=VIDEO_ID`, use `VIDEO_ID`
- **Embed URL format:** `https://www.youtube.com/embed/VIDEO_ID`
- **Start at specific time:** Add `?start=120` (starts at 2 minutes)

---

## Creating Knowledge Base Articles

Knowledge Base articles are technical deep-dives and explanations. Here's how to create them:

### 1. Create the Article File

Create a new markdown file in `public-docs/KNOWLEDGE BASE/`:

```bash
public-docs/KNOWLEDGE BASE/your_article_name.md
```

**Naming convention:**
- Use lowercase with underscores: `memory_management_explained.md`
- Be descriptive: `vulkan_pipeline_setup.md` not `pipeline.md`

### 2. Article Structure Template

```markdown
# Article Title

Brief introduction explaining what this article covers.

## Overview

High-level explanation of the topic.

## Key Concepts

### Concept 1
Explanation...

### Concept 2
Explanation...

## Examples

### Example 1: Basic Usage

```heidic
// Your code example here
```

### Example 2: Advanced Usage

```heidic
// More complex example
```

## Common Patterns

Describe common patterns or best practices...

## Troubleshooting

### Issue 1
**Problem:** Description
**Solution:** How to fix it

## Related Topics

- [Link to related article](path/to/article.md)
- [Link to documentation](path/to/docs.md)

## References

- [External link](https://example.com)
- [Internal documentation](path/to/docs.md)
```

### 3. Add to Navigation

Edit `mkdocs/mkdocs.yml` and add your article to the Knowledge Base section:

```yaml
  - Knowledge Base:
    - Shaders Explained: KNOWLEDGE BASE/shaders_explained.md
    - Stage Field Explanation: KNOWLEDGE BASE/stage_field_explanation.md
    - Your New Article: KNOWLEDGE BASE/your_article_name.md  # Add this line
    - Zero Boilerplate: ZERO BOILERPLATE/ZERO_BOILERPLATE_EXPLAINED.md
```

### 4. Article Best Practices

- **Clear title:** Make it obvious what the article covers
- **Introduction:** Start with a brief overview
- **Code examples:** Use HEIDIC code blocks with ````heidic` syntax
- **Visual aids:** Include diagrams or screenshots when helpful
- **Cross-references:** Link to related documentation
- **Structure:** Use clear headings and subheadings
- **Examples:** Provide both simple and advanced examples
- **Troubleshooting:** Include common issues and solutions

### 5. Example Knowledge Base Article

See existing articles for reference:
- `KNOWLEDGE BASE/shaders_explained.md`
- `KNOWLEDGE BASE/stage_field_explanation.md`

### 6. Testing Your Article

1. Build the docs: `mkdocs build -f mkdocs/mkdocs.yml`
2. Serve locally: `mkdocs serve -f mkdocs/mkdocs.yml`
3. Navigate to your article in the browser
4. Check that all links work and formatting looks good

### 7. Categories

You can organize articles into subdirectories:

```
KNOWLEDGE BASE/
  - graphics/
    - shaders_explained.md
    - rendering_pipeline.md
  - memory/
    - memory_management.md
    - ownership_semantics.md
  - performance/
    - optimization_tips.md
```

Then reference them in `mkdocs.yml`:
```yaml
- Knowledge Base:
  - Graphics:
    - Shaders Explained: KNOWLEDGE BASE/graphics/shaders_explained.md
  - Memory:
    - Memory Management: KNOWLEDGE BASE/memory/memory_management.md
```

