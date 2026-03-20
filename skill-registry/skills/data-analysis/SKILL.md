---
name: data-analysis
description: Analyze datasets and generate insights
---

# Data Analysis

Explore datasets with reproducible steps and honest statistics. Check data quality first: nulls, duplicates, unit mixups, selection bias.

Visualize distributions before jumping to aggregates; document assumptions.

## When to Use
- Exploratory analysis, metric debugging, or experiment readouts

## Workflow
1. Profile schema, row counts, and key dimensions.
2. Clean with logged transformations (don’t silent-drop rows without note).
3. Choose tests/models appropriate to sample size and distribution.
4. Report effect sizes and uncertainty, not only p-values or point estimates.

## Examples
**Example 1:** Conversion funnel: define strict event order; watch timezone boundaries.

**Example 2:** Outliers: investigate before deleting; may be fraud or logging bugs.
