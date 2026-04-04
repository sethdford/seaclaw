---
paths: src/daemon.c, src/gateway/gateway.c, scripts/daemon-*
---

# Daemon & Worker Pool Rules

Background worker pool for job queue processing, resource management, and graceful shutdown. Read `AGENTS.md` section 6 and `docs/standards/operations/daemon.md` before modifying.

## Worker Pool Configuration

```json
{
  "max_parallel": 2,
  "auto_scale": false,
  "auto_scale_interval": 5,
  "max_workers": 8,
  "min_workers": 1,
  "worker_mem_gb": 4,
  "estimated_cost_per_job_usd": 5.0
}
```

- Static mode: `max_parallel` limits concurrent jobs
- Auto-scale: dynamic worker count based on (CPU, memory, budget, queue depth)
- Scaling factors take minimum: `min(cpu_utilization, memory_available, budget_remaining, queue_demand)`

## Job Queueing

- FIFO with priority lanes (urgent, normal, low)
- Deduplication: same issue → reuse in-flight job
- Retry policy: configurable backoff (exponential, max 3 attempts)
- Job timeout: configurable per job type (default 10 min)
- Stale job cleanup: remove jobs older than 24h

## Graceful Shutdown

1. Stop accepting new jobs
2. Wait for in-flight jobs to complete (with timeout)
3. Drain job queue (reschedule remaining)
4. Close database connections
5. Exit (or crash if timeout)

Signals: `SIGTERM` (graceful), `SIGINT` (fast shutdown).

## Monitoring Hooks

- `WorkerStarted` — log worker init
- `JobDequeued` — log job start
- `JobCompleted` — log result, metrics
- `JobFailed` — log error, attempt retry
- `WorkerStopped` — log worker cleanup

## Load Balancing

- Distribute jobs across workers (round-robin)
- Pin long-running jobs to single worker if needed
- Rebalance queue every N seconds (configurable)
- Report metrics: queue_depth, worker_utilization, job_duration_p50/p95

## Rules

- Never block main thread on worker operations
- Use exponential backoff for retries (don't hammer failed services)
- Log all state transitions (start, enqueue, dequeue, complete)
- Health check: POST `/health` every 10 sec (includes worker status)

## Standards

- Read `docs/standards/operations/monitoring.md` for metrics
- Read `docs/standards/engineering/error-handling.md` for retry patterns
