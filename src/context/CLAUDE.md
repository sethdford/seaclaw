# src/context/ — Context Management for Agent Conversations

Context modeling for agent conversations: conversation dynamics, cognitive load, mood, theory of mind, and behavioral extensions. Powers natural dialogue and persona-aware responses.

## Key Files

- `conversation.c` — conversation flow, participation thresholds, keyword/phrase lists, contractions
- `cognitive_load.c` — cognitive load estimation
- `mood.c` — mood state tracking
- `theory_of_mind.c` — user modeling
- `behavioral.c`, `protective.c`, `anticipatory.c` — behavioral extensions
- `event_extract.c` — event extraction from messages

## Rules

- Uses embedded data from `src/data/` (loader, JSON word lists)
- Configurable via `hu_conversation_set_*` functions
- `HU_IS_TEST` guards for side effects; tests use mock/default data
