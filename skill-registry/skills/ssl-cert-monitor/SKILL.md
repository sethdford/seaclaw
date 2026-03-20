---
name: ssl-cert-monitor
description: Monitor SSL/TLS certificate expiration and renewal
---

# Ssl Cert Monitor

Prevent outages from expired certificates. Track leaf and chain expiry; alert with enough lead time for renewal and propagation.

Account for staging/canary hostnames and internal TLS, not only public sites.

## When to Use
- Production SRE checklists, quarterly audits, or after adding new domains

## Workflow
1. Inventory hostnames and ports; include APIs and webhooks.
2. Automated check (OpenSSL or monitoring SaaS) with history.
3. Renew via ACME or vendor process; verify chain on multiple clients.
4. Document on-call steps for emergency renewals.

## Examples
**Example 1:** Alert at 30/14/7 days; escalate at 3 days.

**Example 2:** Wildcard cert: note which SANs are covered before adding subdomains.
