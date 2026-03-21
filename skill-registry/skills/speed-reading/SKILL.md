# Speed Reading (Structured)

Read for **purpose**: different goals need different passes. Optimize for retrieval and correctness, not covering every word.

## When to Use
- Long specs, papers, policies, RFCs, or large codebases when the user needs answers fast or a sane reading plan.

## Workflow
**SQ3R:** **Survey** (skim structure), **Question** (what must you answer?), **Read** (targeted), **Recite** (summarize in your words), **Review** (check against questions).

**Technical docs:** abstract/intro/conclusion first → headings and figures → deep-read only the sections tied to your questions.

**Code:** tests and examples first → public API/types → implementation on demand.

Label the mode explicitly: **reading to find** (locate facts, definitions, constraints) vs **reading to understand** (build a mental model). Preview structure before linear reading; stop once questions are answered unless understanding mode requires integration.

## Examples
**Example 1:** “What breaking changes in v2?” → survey changelog + migration guide headings; read only matching sections; recite as a bullet delta list.

**Example 2:** “How does auth work?” → find entrypoints and tests named `*auth*`; trace call path; defer crypto internals until behavior questions are settled.
