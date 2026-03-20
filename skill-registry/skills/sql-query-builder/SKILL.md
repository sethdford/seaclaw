---
name: sql-query-builder
description: Build and optimize SQL queries from natural language
---

# Sql Query Builder

Translate natural language requests into correct, efficient SQL. Always identify dialect; qualify tables; prefer parameterized queries over string concat.

Check cardinality: joins can explode rows; use `EXPLAIN` when performance matters.

## When to Use
- Ad hoc analytics, BI handoffs, or teaching SQL from English questions

## Workflow
1. Clarify entities, filters, grouping, sorting, and limit.
2. Map to schema; resolve ambiguous column names with aliases.
3. Write parameterized SQL; validate read-only vs write intent.
4. For optimization: indexes, sargable predicates, avoid `SELECT *` in prod paths.

## Examples
**Example 1:** “Top 10 products by revenue last month” → time filter on orders, join line items, `SUM`, `GROUP BY`, `ORDER BY` desc `LIMIT 10`.

**Example 2:** Detect CROSS JOIN risk when multiple fact tables mentioned—decompose or use bridge.
