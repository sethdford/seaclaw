---
title: Incident Response
---

# Incident Response

Standards for detecting, triaging, and resolving incidents across all human deployment contexts (native binary, Docker, WASM, gateway server).

**Cross-references:** [monitoring.md](monitoring.md), [../quality/governance.md](../quality/governance.md)

---

## Severity Levels

| Severity | Definition                               | Response Time | Example                                                                   |
| -------- | ---------------------------------------- | ------------- | ------------------------------------------------------------------------- |
| SEV-1    | Agent non-functional, all users affected | 15 min        | Gateway unreachable, binary crashes on startup, all providers failing     |
| SEV-2    | Major feature broken                     | 1 hour        | Tool execution failing, memory backend down, primary channel unresponsive |
| SEV-3    | Degraded experience                      | 4 hours       | Slow provider responses, minor channel issues, non-critical tool failures |
| SEV-4    | Cosmetic / low impact                    | Next sprint   | UI rendering glitch, typo in help text, non-critical log noise            |

---

## Incident Workflow

```
Detect -> Acknowledge -> Triage -> Mitigate -> Resolve -> Postmortem
```

### 1. Detect

- Automated alerts from `hu_observer_t` hooks (see [monitoring.md](monitoring.md))
- User reports via channel (direct message to agent)
- CI/CD pipeline failures
- Test suite regressions

### 2. Acknowledge

- Confirm the alert is real (not transient network blip)
- Assign severity level
- Start an incident log (timestamp + observations)

### 3. Triage

Diagnose in this order:

1. **Is it a deploy/build issue?** Check if a recent binary change correlates with the error
2. **Is it a provider issue?** Check AI provider status pages; test with fallback provider
3. **Is it a data/memory issue?** Check SQLite integrity, memory backend connectivity
4. **Is it a platform issue?** Check OS-level resources (disk, memory, network)
5. **Is it a configuration issue?** Validate `~/.human/config.json` against schema

### 4. Mitigate

Restore service first, investigate root cause second.

| Scenario                         | Mitigation                                                           |
| -------------------------------- | -------------------------------------------------------------------- |
| Bad binary release               | Roll back to previous binary                                         |
| AI provider outage               | Switch to fallback provider (auto if configured)                     |
| SQLite corruption                | Restore from backup; reinitialize if no backup                       |
| Gateway crash loop               | Check logs; restart with `--verbose`; isolate the triggering request |
| Memory exhaustion                | Check for leaks (ASan); reduce concurrent sessions; increase limits  |
| Security policy misconfiguration | Revert to default policy; validate config                            |

### 5. Resolve

- Confirm service is healthy (test a round-trip message through primary channel)
- Notify affected users if appropriate
- Close the incident log

### 6. Postmortem

Required for SEV-1 and SEV-2. Written within 48 hours.

Template:

```markdown
## Incident: [Title]

**Date:** YYYY-MM-DD
**Severity:** SEV-X
**Duration:** Xh Ym
**Deployment context:** [native / docker / wasm / gateway]

### Summary

One paragraph: what happened, who was affected, how it was resolved.

### Timeline

- HH:MM -- Event
- HH:MM -- Event

### Root Cause

What actually broke and why.

### What Went Well

What helped detect and resolve quickly.

### What Went Wrong

What slowed detection or resolution.

### Action Items

- [ ] Specific, assignable tasks to prevent recurrence
- [ ] Tests to add that would have caught this
- [ ] Monitoring improvements
```

---

## Runbooks

Maintain runbooks for common scenarios in `docs/runbooks/`:

| Runbook                     | Covers                                                      |
| --------------------------- | ----------------------------------------------------------- |
| `provider-outage.md`        | Fallback behavior, provider health checks, manual override  |
| `gateway-crash.md`          | Log analysis, request isolation, restart procedures         |
| `memory-backend-failure.md` | SQLite recovery, LanceDB reconnection, graceful degradation |
| `binary-size-regression.md` | Bisecting the cause, feature flag isolation                 |
| `security-incident.md`      | Credential rotation, audit trail, disclosure process        |

---

## Communication

During SEV-1/SEV-2:

- Post status to team channel immediately
- Update every 30 minutes until resolved
- Send all-clear when confirmed stable for 15 minutes
- For gateway deployments: consider a status endpoint response indicating degraded state

---

## Anti-Patterns

```
WRONG -- Investigate root cause while users are down
RIGHT -- Mitigate first (restore service), then investigate

WRONG -- Skip postmortem because "it was a simple fix"
RIGHT -- Every SEV-1/2 gets a postmortem. Simple fixes often mask systemic issues.

WRONG -- Assign severity based on how many people complain
RIGHT -- Assign severity based on impact scope and service health

WRONG -- Fix the incident but add no tests
RIGHT -- Every incident produces at least one new test that would have caught it
```
