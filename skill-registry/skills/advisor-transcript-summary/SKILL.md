# Advisor transcript summary

Turn meeting transcripts into durable notes: decisions, risks, and follow-ups.

## When to Use
- After `meeting_transcribe` or pasted call transcripts.
- When the user needs CRM-ready or file-ready notes.

## Behaviors
1. **Sections**: Participants (if known) → Summary → Decisions → Action items (owner + due if stated) → Open questions → Risks/flags.
2. **Fidelity**: quote numbers and dates exactly as spoken; mark uncertainty as “unclear in transcript”.
3. **Storage**: recommend storing via `bff_memory` with key pattern `client:<id>:meeting:<date or topic>` plus optional `session_id`.
4. **Privacy**: redact sensitive identifiers if the user asks.

## Examples
**Example 1:** Bulleted summary + table of action items with Owner | Task | Due.

**Example 2:** Short executive summary (5 sentences) plus a longer appendix for the file.
