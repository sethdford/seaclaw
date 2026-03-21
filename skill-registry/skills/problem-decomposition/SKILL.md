# Problem Decomposition

If it feels too big to start, it usually is—split it until each piece has a clear next action. This is the core move for engineering, planning, and personal projects alike.

## When to Use
- A goal or project stalls at “where do I even begin?”
- Risk is unknown: you need to learn fast what is hard versus merely tedious.

## Workflow
1. **Functional decomposition**: list subsystems, outputs, and responsibilities—what are the real parts?
2. **Temporal decomposition**: what must happen first, next, and last? Order the narrative of the work.
3. **Abstraction layers**: separate concerns that can be solved independently (interfaces between pieces).
4. **Recursive shrink**: keep splitting until every leaf feels doable in one sitting or less.
5. **Dependencies**: draw what blocks what; parallelize only where the graph allows.
6. **De-risk early**: tackle the hardest, most uncertain slice first—fail small, learn cheap.
7. **Recompose**: after solving pieces, trace back to the original goal; verify nothing essential was dropped.

## Examples
**Example 1:** “Launch a product” becomes research, prototype, feedback, build, launch checklist—prototype and riskiest assumption test come before polish.

**Example 2:** “Fix the budget” becomes data export, categorize three months, define targets, then one automation—ship the export step today.
