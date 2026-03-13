# src/intelligence/ — Intelligence Engines

Four SQLite-backed engines that close the learning loop: observe → learn → adapt → improve.
All gated behind `HU_ENABLE_SQLITE`.

## Architecture

```
online_learning.c   EMA-weighted behavioral signals (tool success, user feedback)
self_improve.c      Reflection → prompt patch pipeline
value_learning.c    Learns user values from corrections/approvals
world_model.c       Causal world model (simulate, counterfactual, rank actions)
```

## Common Pattern

Each engine follows `create → init_tables → operations → deinit`:

- `hu_<engine>_create(alloc, db, out)` — initialize with allocator and SQLite handle
- `hu_<engine>_init_tables(engine)` — create tables idempotently
- `hu_<engine>_deinit(engine)` — no-op (caller owns db)

## Key APIs

**Online Learning**: `record` signals, `update_weight` with EMA, `get_weight`, `build_context` for prompt injection.

**Self-Improvement**: `apply_reflections` converts `self_evaluations` rows into `prompt_patches`, `get_prompt_patches` joins active patches for system prompt.

**Value Learning**: `learn_from_correction` / `learn_from_approval`, `weaken`, `list`, `build_prompt`, `alignment_score`.

**World Model**: `record_outcome`, `simulate`, `counterfactual`, `evaluate_options` (rank actions), `causal_depth`.

## Rules

- Caller owns the `sqlite3 *db` handle — engines never open/close it
- All error codes use `HU_ERR_MEMORY_STORE` for SQLite failures (not `HU_ERR_MEMORY_BACKEND`)
- Copy `sqlite3_column_text` results before `sqlite3_finalize` — pointers dangle after finalize
- Use `SQLITE_STATIC` (not `SQLITE_TRANSIENT`) for string bindings
- All allocations through `hu_allocator_t` — never raw malloc/free
