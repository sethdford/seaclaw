# Technical Writing

Produce docs that match the reader’s **skill level**, reveal information **progressively**, and favor **working examples** over abstract prose. Clarity beats completeness on page one.

**Tags:** cognitive, writing, communication

## When to Use
- READMEs, runbooks, API docs, architecture notes, internal wikis, or any “how does this work?” artifact.

## Behaviors
**Audience:** State who it’s for (beginner vs operator vs contributor). **Structure:** **Overview** (what and why) → **Quickstart** (running in ~**5 minutes**) → **Reference** (full detail) → **Troubleshooting** (common failures). **Progressive disclosure:** lead with outcomes and paths; push edge cases lower. **Style:** one idea per paragraph; prefer active voice; define jargon once; state the takeaway first (no buried lede). **Code:** copy-pasteable, minimal, tested or clearly marked if illustrative. **Avoid:** passive mush, assumed context, jargon without definition.

## Examples
**Example 1:** New service doc opens with “what it does,” then a 5-step quickstart, then flags/env tables, then “If you see 403…” with causes.

**Example 2:** API page puts one happy-path request/response up top; pagination and error codes live in reference below—not in the first paragraph.
