---
name: csv-transformer
description: Transform, filter, and reshape CSV data
---

# Csv Transformer

Reshape CSV/TSV reliably with encoding and delimiter awareness. Stream large files; avoid loading multi-GB into memory.

Preserve quoting rules for fields with commas/newlines.

## When to Use
- ETL prep, column renames, joins on keys, or export to another tool

## Workflow
1. Detect encoding (UTF-8 BOM, Latin-1); validate header row uniqueness.
2. Define key columns and join strategy; handle missing keys explicitly.
3. Apply transforms as pure functions; write tests on sample rows.
4. Validate output row counts and checksums vs expectations.

## Examples
**Example 1:** Pivot long to wide: aggregate duplicates with sum/max rules per metric.

**Example 2:** Merge customer CSV with orders on `customer_id`; report unmatched rates.
