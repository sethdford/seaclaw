---
title: Observability and Service Level Objectives
updated: 2026-03-13
---

# Observability and Service Level Objectives

SLI/SLO definitions, structured logging, metrics, and trace propagation for the human runtime.

**Cross-references:** [monitoring.md](monitoring.md), [incident-response.md](incident-response.md), [../engineering/performance.md](../engineering/performance.md)

---

## 1. Observability Pillars

| Pillar      | Implementation                             | Status      |
| ----------- | ------------------------------------------ | ----------- |
| **Logs**    | `hu_observer_t` vtable, structured JSON    | Implemented |
| **Metrics** | `hu_observer_t` counters/gauges/histograms | Implemented |
| **Traces**  | Request ID propagation through agent turns | Partial     |

## 2. Service Level Indicators (SLIs)

| SLI                       | Definition                                    | Measurement                                 |
| ------------------------- | --------------------------------------------- | ------------------------------------------- |
| **Gateway availability**  | % of health checks returning 200 in a window  | `GET /health` probe every 30s               |
| **Request latency (p99)** | 99th percentile agent turn latency            | Timer from message receipt to response send |
| **Error rate**            | % of agent turns returning non-OK error       | `hu_error_t != HU_OK` count / total turns   |
| **Provider success rate** | % of provider API calls succeeding            | Per-provider success/total counters         |
| **Memory query latency**  | p99 time for memory recall operations         | Timer around `mem->vtable->recall()`        |
| **Channel delivery rate** | % of outbound messages successfully delivered | Per-channel send success/total              |

## 3. Service Level Objectives (SLOs)

| SLO                   | Target             | Error Budget (30-day)           | Alerting                       |
| --------------------- | ------------------ | ------------------------------- | ------------------------------ |
| Gateway availability  | 99.9%              | 43.2 minutes downtime           | Alert at 99.5% (burn rate 2x)  |
| Request latency (p99) | < 5s               | N/A (latency, not availability) | Alert if p99 > 10s for 5 min   |
| Error rate            | < 1%               | 432 errors per 43,200 requests  | Alert at 2% sustained 5 min    |
| Provider success rate | > 95% per provider | 5% failures acceptable          | Alert at 10% per provider      |
| Memory query latency  | < 100ms p99        | N/A                             | Alert if p99 > 500ms for 5 min |

### Error Budget Policy

When the error budget is exhausted (SLO breached):

1. Halt non-critical feature work
2. Prioritize reliability improvements
3. Postmortem required for any SLO breach > 1 hour
4. Resume normal development when budget is positive for 7 consecutive days

## 4. Structured Logging

### 4.1 Log Schema

All log entries use structured JSON format:

```json
{
  "ts": "2026-03-13T10:15:30.123Z",
  "level": "error",
  "module": "provider",
  "msg": "API request failed",
  "request_id": "req_abc123",
  "provider": "openrouter",
  "model": "claude-sonnet-4",
  "error_code": "HU_ERR_NETWORK",
  "latency_ms": 5032
}
```

### 4.2 Required Fields

| Field        | Type     | Required       | Description                                  |
| ------------ | -------- | -------------- | -------------------------------------------- |
| `ts`         | ISO 8601 | Yes            | Timestamp with milliseconds                  |
| `level`      | enum     | Yes            | `debug`, `info`, `warn`, `error`             |
| `module`     | string   | Yes            | Source module (provider, memory, tool, etc.) |
| `msg`        | string   | Yes            | Human-readable message                       |
| `request_id` | string   | When available | Correlation ID for request tracing           |

### 4.3 Prohibited Fields

Never log: API keys, tokens, user messages (content), PII, file contents. See `../security/data-privacy.md`.

## 5. Metrics

### 5.1 Counter Metrics

| Metric                     | Labels                        | Description             |
| -------------------------- | ----------------------------- | ----------------------- |
| `hu_requests_total`        | `channel`, `status`           | Total agent turns       |
| `hu_provider_calls_total`  | `provider`, `model`, `status` | Provider API calls      |
| `hu_tool_executions_total` | `tool`, `status`              | Tool invocations        |
| `hu_memory_ops_total`      | `operation`, `backend`        | Memory store/recall ops |

### 5.2 Histogram Metrics

| Metric                   | Buckets (ms)                      | Description           |
| ------------------------ | --------------------------------- | --------------------- |
| `hu_request_duration_ms` | 100, 500, 1000, 2000, 5000, 10000 | Agent turn duration   |
| `hu_provider_latency_ms` | 100, 500, 1000, 2000, 5000        | Provider API latency  |
| `hu_memory_query_ms`     | 10, 50, 100, 500                  | Memory recall latency |

### 5.3 Gauge Metrics

| Metric               | Description                    |
| -------------------- | ------------------------------ |
| `hu_active_sessions` | Currently active chat sessions |
| `hu_memory_entries`  | Total memory entries in store  |
| `hu_rss_bytes`       | Current resident set size      |

## 6. Trace Propagation

### 6.1 Request ID

Every inbound message generates a unique `request_id` that propagates through:

1. Channel receive → Dispatcher → Agent turn → Provider call → Tool execution → Channel send

All log entries and metrics within a turn include the `request_id`.

### 6.2 Future: OpenTelemetry

When OpenTelemetry integration is added:

- Use W3C Trace Context (`traceparent` header) for HTTP
- Span hierarchy: `channel.receive` > `agent.turn` > `provider.chat` / `tool.execute`
- Export via OTLP to any compatible backend

## 7. Health Checks

### 7.1 Gateway Health

`GET /health` returns:

```json
{
  "status": "ok",
  "uptime_seconds": 3600,
  "version": "2026.3.13",
  "memory_backend": "sqlite",
  "active_channels": 3
}
```

### 7.2 Deep Health Check

`GET /health?deep=true` additionally checks:

- SQLite database accessible
- At least one provider configured and reachable
- Memory subsystem operational

## Normative References

| ID             | Source                                                  | Version              | Relevance                                |
| -------------- | ------------------------------------------------------- | -------------------- | ---------------------------------------- |
| [SRE-Book]     | Google Site Reliability Engineering                     | 1st ed. (2016)       | SLI/SLO/error budget methodology (Ch. 4) |
| [SRE-Workbook] | Google SRE Workbook                                     | 1st ed. (2018)       | SLO implementation patterns (Ch. 2)      |
| [OTel-Spec]    | OpenTelemetry Specification                             | 1.x (2024)           | Trace context, metric semantics          |
| [W3C-TC]       | W3C Trace Context                                       | Level 1 (2021-11-23) | `traceparent` header propagation         |
| [Obs-Eng]      | Majors, Fong-Jones, Miranda — Observability Engineering | 1st ed. (2022)       | Observability philosophy and practices   |
