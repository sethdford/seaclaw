---
title: Performance Testing
updated: 2026-03-13
---

# Performance Testing

Methodology for performance measurement, regression detection, and profiling across all human surfaces.

**Cross-references:** [performance.md](performance.md), [../operations/observability.md](../operations/observability.md), [../quality/ceremonies.md](../quality/ceremonies.md)

---

## 1. Performance Testing Tiers

| Tier                  | What                                | When                     | Gate                    |
| --------------------- | ----------------------------------- | ------------------------ | ----------------------- |
| **Micro-benchmark**   | Individual function timing          | Development              | Informational           |
| **Binary regression** | Size, startup, RSS                  | Every CI build           | Block if >5% regression |
| **Load test**         | Gateway concurrent connections, RPS | Pre-release              | Block if p99 >10s       |
| **UI performance**    | Lighthouse, Core Web Vitals         | Every PR (ui/, website/) | Block if Lighthouse <95 |
| **Profile**           | CPU flame graph, memory allocation  | On-demand / quarterly    | Informational           |

## 2. Binary Regression Testing

### 2.1 Metrics Tracked

| Metric                | Baseline | Regression Threshold | Measurement                          |
| --------------------- | -------- | -------------------- | ------------------------------------ |
| Binary size (release) | ~1696 KB | +5% (85 KB)          | `ls -la build-release/human`         |
| Text section          | 480 KB   | +10% (48 KB)         | `size build-release/human`           |
| Cold start            | <30 ms   | +50% (45 ms)         | `/usr/bin/time -l ./human --version` |
| Peak RSS              | ~5.7 MB  | +20% (1.14 MB)       | `/usr/bin/time -l ./human --version` |

### 2.2 CI Enforcement

The `benchmark.yml` workflow measures these on every push to main:

```yaml
- name: Measure binary size
  run: |
    size=$(stat -f%z build-release/human)
    echo "binary_size=$size" >> $GITHUB_OUTPUT
    if [ "$size" -gt "$((BASELINE * 105 / 100))" ]; then
      echo "::error::Binary size regression: $size bytes (baseline: $BASELINE)"
      exit 1
    fi
```

### 2.3 Justified Growth

When a feature legitimately increases binary size:

1. Measure the exact delta
2. Document the justification in the commit message
3. Update the baseline in `benchmark.yml`
4. Ensure the feature is compile-flag gated if >10 KB

## 3. Gateway Load Testing

### 3.1 Test Scenarios

| Scenario         | Description                            | Target                        |
| ---------------- | -------------------------------------- | ----------------------------- |
| Single user      | 1 connection, sequential messages      | p99 <2s response              |
| Concurrent users | 10 connections, parallel messages      | p99 <5s response              |
| Burst            | 50 messages in 1 second                | No crashes, graceful queueing |
| Long session     | 1 connection, 100 messages over 10 min | No memory leak (RSS stable)   |
| Idle             | Gateway running 24h with no traffic    | RSS stable, no growth         |

### 3.2 Tools

| Tool                                   | Purpose                                    |
| -------------------------------------- | ------------------------------------------ |
| `wrk` / `wrk2`                         | HTTP load testing with constant throughput |
| `websocat`                             | WebSocket load testing (manual)            |
| Custom script (`scripts/load-test.sh`) | Gateway-specific test harness              |

### 3.3 Memory Leak Detection

```bash
# Run gateway for N requests, compare RSS before and after
initial_rss=$(ps -o rss= -p $PID)
# ... send 1000 requests ...
final_rss=$(ps -o rss= -p $PID)
growth=$((final_rss - initial_rss))
# Growth should be <1 MB for 1000 requests
```

## 4. UI Performance Testing

### 4.1 Lighthouse CI

Every PR touching `ui/` or `website/` runs Lighthouse CI:

| Metric         | Dashboard | Website |
| -------------- | --------- | ------- |
| Performance    | >= 95     | >= 95   |
| Accessibility  | >= 98     | >= 98   |
| Best Practices | >= 95     | >= 95   |
| SEO            | >= 90     | >= 95   |

### 4.2 Core Web Vitals

| Metric | Target | Stretch | Measurement             |
| ------ | ------ | ------- | ----------------------- |
| LCP    | <1.5s  | <0.5s   | Lighthouse + field data |
| CLS    | <0.05  | 0.00    | Lighthouse + field data |
| INP    | <200ms | <50ms   | Field data only         |
| FCP    | <1.0s  | <0.5s   | Lighthouse              |
| TBT    | <200ms | 0ms     | Lighthouse              |

### 4.3 Bundle Size Budget

| Chunk              | Budget         | Measurement         |
| ------------------ | -------------- | ------------------- |
| Initial bundle     | <150 KB (gzip) | `vite build` output |
| Lazy view chunk    | <100 KB (gzip) | `vite build` output |
| Total (all chunks) | <500 KB (gzip) | `vite build` output |

## 5. Profiling Workflow

### 5.1 CPU Profiling

**macOS (Instruments):**

```bash
xcrun xctrace record --template "Time Profiler" --launch ./human -- agent -m "test"
```

**Linux (perf):**

```bash
perf record -g ./human agent -m "test"
perf report
```

### 5.2 Memory Profiling

**ASan (always-on in dev):**

```bash
cmake --preset dev  # Enables ASan
./build/human_tests  # Reports leaks
```

**Valgrind (deep analysis):**

```bash
valgrind --tool=massif ./human agent -m "test"
ms_print massif.out.*
```

### 5.3 Web UI Profiling

```bash
# Browser DevTools Performance tab
# Or automated via Lighthouse CI user flows
npx lighthouse http://localhost:5173 --output=json --output-path=./lighthouse.json
```

## 6. Quarterly Performance Review

Per `docs/standards/quality/ceremonies.md`, the quarterly release gate includes:

1. Binary size within budget
2. Startup time within budget
3. RSS within budget
4. Lighthouse scores meet thresholds
5. No open memory leaks (ASan clean)
6. Load test scenarios pass

Document results in the quality scorecard (`docs/quality-scorecard.md`).

## Normative References

| ID           | Source                              | Version          | Relevance                             |
| ------------ | ----------------------------------- | ---------------- | ------------------------------------- |
| [WebVitals]  | Google Web Vitals                   | 2024 definitions | LCP, CLS, INP measurement methodology |
| [Gregg-SP]   | Brendan Gregg — Systems Performance | 2nd ed. (2020)   | Performance analysis methodology      |
| [Lighthouse] | Google Lighthouse                   | v12              | Automated web performance auditing    |
| [ASan]       | Google AddressSanitizer             | LLVM 18          | Memory error and leak detection       |
| [wrk2]       | wrk2 — HTTP benchmarking tool       | v4.2             | Constant-throughput HTTP load testing |
