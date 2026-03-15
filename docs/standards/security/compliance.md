---
title: Data Privacy Compliance
updated: 2026-03-13
---

# Data Privacy Compliance

Regulatory compliance mapping for data privacy across GDPR, CCPA/CPRA, and related frameworks.

**Cross-references:** [data-privacy.md](data-privacy.md), [threat-model.md](threat-model.md), [../operations/incident-response.md](../operations/incident-response.md)

---

## 1. Scope

This standard covers human's obligations as a **data processor** when deployed by users (data controllers). human processes:

- User messages (text, voice transcriptions)
- Memory entries (stored conversations, facts, preferences)
- Configuration (API keys, channel credentials)
- Metadata (timestamps, session IDs, channel identifiers)

## 2. GDPR Compliance Map

| GDPR Article | Requirement                        | human Implementation                                                | Status              |
| ------------ | ---------------------------------- | ------------------------------------------------------------------- | ------------------- |
| Art. 5(1)(a) | Lawfulness, fairness, transparency | AI disclosure standard (`disclosure.md`); no hidden data collection | Done                |
| Art. 5(1)(b) | Purpose limitation                 | Data used only for configured agent tasks                           | Done                |
| Art. 5(1)(c) | Data minimization                  | Memory hygiene auto-archives stale data; configurable retention     | Done                |
| Art. 5(1)(e) | Storage limitation                 | `hygiene_enabled` + configurable `retention_days`                   | Done                |
| Art. 5(1)(f) | Integrity and confidentiality      | ChaCha20-Poly1305 encrypted secrets; TLS-only outbound              | Done                |
| Art. 15      | Right of access                    | `human memory list` exports all stored data                         | Done                |
| Art. 17      | Right to erasure                   | `human memory forget` + bulk delete                                 | Done                |
| Art. 20      | Data portability                   | `human memory export` (JSON format)                                 | Done                |
| Art. 25      | Data protection by design          | Zero-dependency architecture minimizes attack surface               | Done                |
| Art. 32      | Security of processing             | Sandbox isolation, encrypted secrets, audit logging                 | Done                |
| Art. 33      | Breach notification                | Incident response standard defines SEV-1 notification within 72h    | Standard            |
| Art. 35      | DPIA                               | Required for deployments processing sensitive data                  | User responsibility |

## 3. CCPA/CPRA Compliance Map

| Requirement                 | Description                                 | human Implementation                                           |
| --------------------------- | ------------------------------------------- | -------------------------------------------------------------- |
| Right to Know               | Disclose what personal info is collected    | Memory list + config export                                    |
| Right to Delete             | Delete personal information on request      | Memory forget + config purge                                   |
| Right to Opt-Out            | Opt out of sale/sharing of personal info    | human never sells or shares data — single-binary, no telemetry |
| Right to Non-Discrimination | Equal service regardless of privacy choices | N/A — no service tiers                                         |
| Data Minimization           | Collect only necessary data                 | Memory hygiene, configurable retention                         |

## 4. Data Flow Diagram

```
User Message → Channel → Dispatcher → Agent Turn → Provider API (outbound)
                                         ↓
                                    Memory Store (SQLite/local)
                                         ↓
                                    Response → Channel → User
```

**PII touchpoints:**

1. **Channel receive**: raw user message (text/voice)
2. **Provider API call**: message sent to external AI provider
3. **Memory store**: conversation stored locally
4. **Logs**: structured logs (must NOT contain message content)

## 5. Data Processing Agreement (DPA) Requirements

When users deploy human with external AI providers, they must ensure:

- Provider offers a DPA covering GDPR Art. 28 requirements
- Provider does not train on API inputs (or offers opt-out)
- Provider data retention is known and acceptable
- Provider infrastructure meets geographic requirements (EU data residency if needed)

human's documentation (`docs/guides/`) should advise users on provider DPA status.

## 6. Data Retention Defaults

| Data Type        | Default Retention          | Configurable                 | Deletion Method              |
| ---------------- | -------------------------- | ---------------------------- | ---------------------------- |
| Chat messages    | Indefinite (user controls) | Yes (`retention_days`)       | `human memory forget`        |
| Memory entries   | Subject to hygiene cycle   | Yes                          | Auto-archive + manual forget |
| API keys         | Until removed              | N/A                          | `human config set`           |
| Audit logs       | 90 days                    | Yes (`audit.retention_days`) | Automatic rotation           |
| Session metadata | 30 days                    | Yes                          | Automatic cleanup            |

## 7. Audit Trail

All data operations are logged in the audit trail when `security.audit.enabled = true`:

- Memory store/recall/forget operations
- Config changes
- Channel connect/disconnect events
- Provider API calls (metadata only, not content)

Audit trail entries are signed and tamper-evident.

## Normative References

| ID          | Source                                       | Version                        | Relevance                                        |
| ----------- | -------------------------------------------- | ------------------------------ | ------------------------------------------------ |
| [GDPR]      | EU General Data Protection Regulation        | 2016/679                       | Primary EU data protection law                   |
| [CCPA]      | California Consumer Privacy Act              | 2018 (as amended by CPRA 2020) | California data protection                       |
| [ISO-27701] | ISO/IEC 27701 Privacy Information Management | 2019                           | Privacy management system extension to ISO 27001 |
| [NIST-PF]   | NIST Privacy Framework                       | 1.0 (2020-01-16)               | Privacy risk management                          |
| [SOC2-TSC]  | AICPA Trust Services Criteria                | 2017 (with 2022 updates)       | Privacy and security criteria for SOC 2          |
