# HEIDIC Syntax Coloring (H_SCRIBE)

Palette (sRGB):
- Keywords: `rgb(180,140,255)`
- Types: `rgb(120,200,255)`
- Numbers: `rgb(255,200,120)`
- Strings: `rgb(140,220,140)`
- Comments: `rgb(120,140,160)`
- Annotations (@hot, etc.): `rgb(255,180,120)`
- Identifiers/default text: `rgb(230,230,230)`

Token rules (as implemented in `H_SCRIBE/main.py`):
- Line comments: `// ...` → comment color.
- Strings: double-quoted segments, escapes are consumed but not highlighted specially.
- Keywords: `fn, let, if, else, while, for, return, struct, enum, system, extern, import, as, break, continue, true, false`.
- Types: `i32, i64, u32, u64, f32, f64, bool, void, String`.
- Numbers: decimal (`42`, `3.14`) or hex (`0xFF`).
- Annotations: words starting with `@` (e.g., `@hot`).
- Everything else: default text color.

Notes:
- This is a lightweight, non-lexing highlighter aimed at the examples; extend the keyword/type lists as the language grows.
- The highlighter tokenizes per line and does not support multi-line strings or block comments (if added later).

