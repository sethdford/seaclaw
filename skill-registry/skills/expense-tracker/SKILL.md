---
name: expense-tracker
description: Track expenses and categorize spending
---

# Expense Tracker

Categorize spending and surface totals without losing receipt fidelity. Use consistent categories aligned with tax or reimbursement policy when relevant.

Round-trip to CSV or accounting export should preserve original currency and dates.

## When to Use
- Monthly close, reimbursement prep, or budget reviews

## Workflow
1. Ingest transactions (CSV, bank export, photos); normalize dates and currency.
2. Apply merchant rules; flag uncategorized items for human review.
3. Produce summaries by category, period, and project/tag.
4. Store receipts with immutable links to transactions.

## Examples
**Example 1:** Tag “travel:flight” vs “travel:hotel” for per-diem reporting.

**Example 2:** Split one charge across two projects with percentage allocation.
