---
paths:
  - src/ml/**
  - include/human/ml/**
---

# ML Subsystem Rules

## Architecture

- Gated behind `HU_ENABLE_ML`. All ML code must be conditionally compiled.
- Vtable-driven: `hu_model_t`, `hu_ml_optimizer_t` vtable interfaces.
- Read `src/ml/CLAUDE.md` for module architecture.

## Standards

- Read `docs/standards/engineering/testing.md` before adding tests.
- Read `docs/standards/engineering/memory-management.md` — ML tensors must be explicitly freed.
- Read `docs/standards/engineering/performance.md` — measure binary size impact of ML additions.

## Rules

- All tensor allocations go through `hu_allocator_t`. No bare `malloc`.
- Training loops must have `HU_IS_TEST` guards — no real computation in tests.
- Gradient checks must be structural (verify existence and shape), not numerical (fragile).
- Test file: `tests/test_ml.c`.
