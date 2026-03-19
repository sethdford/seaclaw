# src/visual/ — Visual Content Indexing

Visual content indexing and search. Builds SQL queries for image/video metadata stored in the memory backend.

## Key Files

- `content.c` — Content type detection, metadata extraction, and SQL query builders for visual content search

## Rules

- Use parameterized SQL (never string-interpolate user input)
- Content detection must handle missing/corrupt files gracefully
