# src/visual/ — Visual Content Indexing

Visual content indexing and search. Builds SQL queries for image/video metadata stored in the memory backend.

## Key Files

- `visual_index.c` — SQL query builders for visual content search
- `visual_content.c` — Content type detection and metadata extraction

## Rules

- Use parameterized SQL (never string-interpolate user input)
- Content detection must handle missing/corrupt files gracefully
