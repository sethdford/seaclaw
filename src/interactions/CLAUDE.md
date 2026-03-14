# src/interactions/ — User Interaction Tracking

User choice prompts and confirmations. CLI-driven: prints options to stdout, reads from stdin. Used for approval flows and interactive decisions.

## Key Files

- `choices.c` — `hu_choices_prompt`, `hu_choices_confirm`; multi-choice and yes/no prompts

## Rules

- `HU_IS_TEST`: returns default choice without stdin; no real I/O
- Non-test: uses `printf`/`fgets` for interactive prompts
- Keep `HU_CHOICE_BUF_SIZE` (64) for input buffer
