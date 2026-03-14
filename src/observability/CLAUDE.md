# src/observability/ — Logging and Metrics Observers

Observability hooks implementing `hu_observer_t`. JSON log output, OpenTelemetry export, BTH metrics, and multi-observer fan-out.

## Key Files

- `log_observer.c` — JSON-structured log events to stderr/file
- `multi_observer.c` — fans out to multiple observers
- `otel.c` — OpenTelemetry export (when enabled)
- `metrics_observer.c` — metrics aggregation
- `bth_metrics.c` — BTH-specific metrics

## Rules

- Never log secrets, tokens, or request bodies
- Use `hu_observer_event_t` tag for event type
- Scrub PII in provider/scrub before observer dispatch
