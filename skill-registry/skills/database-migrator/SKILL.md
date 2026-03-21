---
name: database-migrator
description: Generate and run database schema migrations
---

# Database Migrator

Manage schema changes safely with forward and backward compatibility in mind. Prefer additive migrations first, then data backfills, then deprecations—never drop columns in the same release that still reads them.

Always test migrations against a copy of production-like data size when practical.

## When to Use
- Adding tables/columns/indexes, performance tuning, or data backfills

## Workflow
1. Model current schema and desired state; estimate row counts and lock duration.
2. Write reversible steps: expand (add nullable column), backfill in batches, contract (add constraints, drop old).
3. Add indexes concurrently when the DB supports it; avoid full-table locks on large tables.
4. Verify rollback path or document irreversible steps explicitly.

## Examples
**Example 1:** Rename column: add new column, dual-write, backfill, switch reads, drop old.

**Example 2:** Add NOT NULL: add with default or backfill in chunks before enforcing constraint.
