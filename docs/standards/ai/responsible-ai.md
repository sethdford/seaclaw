---
title: Responsible AI and Model Governance
updated: 2026-03-13
---

# Responsible AI and Model Governance

Governance framework for AI model selection, evaluation, safety, and responsible deployment in the human runtime.

**Cross-references:** [evaluation.md](evaluation.md), [hallucination-prevention.md](hallucination-prevention.md), [../security/ai-safety.md](../security/ai-safety.md), [disclosure.md](disclosure.md)

---

## 1. Principles

These principles are derived from the NIST AI Risk Management Framework and adapted to human's architecture:

1. **Transparency**: human always discloses its AI nature when asked (see `disclosure.md`)
2. **Accountability**: every AI-generated output is traceable to a provider, model, and prompt
3. **Fairness**: output quality must not systematically vary by user identity or demographic
4. **Safety**: harmful outputs are filtered before delivery (see `ai-safety.md`)
5. **Privacy**: user data is processed per `data-privacy.md`; no training on user conversations
6. **Robustness**: provider failover ensures service continuity; no single point of failure

## 2. Model Selection Criteria

When choosing a default or recommended model, evaluate across these dimensions:

| Dimension  | Weight | Measurement                                             |
| ---------- | ------ | ------------------------------------------------------- |
| Capability | 30%    | MMLU, HumanEval, HELM composite score                   |
| Safety     | 25%    | TruthfulQA, red-team pass rate, refusal appropriateness |
| Latency    | 20%    | p50 and p99 time-to-first-token                         |
| Cost       | 15%    | $/1M tokens (input + output weighted)                   |
| Privacy    | 10%    | Data retention policy, training data opt-out            |

### Minimum Requirements

- TruthfulQA score >= 0.6
- Supports system prompt with tool-calling
- Provider offers data processing agreement (DPA)
- No known training on user conversation data without opt-in

## 3. Provider Evaluation

### 3.1 Golden Set Testing

Maintain a golden set of 50+ test conversations covering:

- Factual accuracy (verifiable claims)
- Tool dispatch (correct tool selection and parameter formatting)
- Persona fidelity (style matching)
- Safety boundaries (refusal of harmful requests)
- Edge cases (empty input, very long input, multilingual)

Run the golden set against each provider monthly. Track scores over time. Regression > 10% triggers investigation.

### 3.2 A/B Evaluation

For model upgrades or new provider onboarding:

- Minimum 20 evaluation outputs
- Minimum 5% improvement on golden set composite score to justify switch
- No regression on safety dimension

## 4. Bias Detection

### 4.1 Evaluation Protocol

Quarterly, evaluate model outputs for:

- **Demographic parity**: equal quality across simulated user identities
- **Stereotype reinforcement**: flag outputs that reinforce harmful stereotypes
- **Language equity**: response quality across English, Spanish, Mandarin, Arabic (top 4 user languages)

### 4.2 Mitigation

- System prompts include explicit fairness instructions
- Persona system never includes demographic assumptions
- Tool execution is identity-blind (same policy for all users)

## 5. Output Monitoring

### 5.1 Safety Filters

Before delivery to any channel, outputs pass through:

1. **Content classification**: toxic/harmful content detection
2. **PII detection**: prevent accidental PII leakage in responses
3. **Hallucination flags**: low-confidence claims marked with hedging language

### 5.2 Feedback Loop

- Users can flag problematic outputs (channel-specific mechanism)
- Flagged outputs are logged (without PII) for review
- Patterns in flags trigger golden set expansion

## 6. Incident Response

When an AI safety incident occurs (harmful output delivered, data leak, bias pattern discovered):

1. Severity classification per `docs/standards/operations/incident-response.md`
2. Immediate: disable the responsible model/provider if severity >= SEV-2
3. Investigation: root cause analysis (prompt? model? filter gap?)
4. Remediation: golden set expansion, filter update, or provider switch
5. Postmortem: published per incident response standard

## 7. Documentation Requirements

Every provider integration must document:

- Model name and version
- Provider's data retention policy
- Whether the provider trains on API inputs
- Known limitations and failure modes
- Cost per 1M tokens

## Normative References

| ID              | Source                               | Version                 | Relevance                                    |
| --------------- | ------------------------------------ | ----------------------- | -------------------------------------------- |
| [NIST-AI-RMF]   | NIST AI Risk Management Framework    | 1.0 (2023-01-26)        | Risk categorization and governance structure |
| [EU-AI-Act]     | EU Artificial Intelligence Act       | 2024/1689               | Regulatory framework for AI systems          |
| [Anthropic-RSP] | Anthropic Responsible Scaling Policy | v1.0 (2023-09)          | Capability evaluation and safety thresholds  |
| [Google-SAIF]   | Google Secure AI Framework           | 1.0 (2023-06)           | AI system security best practices            |
| [HELM]          | Stanford HELM Benchmark              | v1.0                    | Holistic evaluation methodology              |
| [TruthfulQA]    | Lin et al., TruthfulQA               | 2021 (arXiv:2109.07958) | Truthfulness measurement                     |
| [OWASP-LLM]     | OWASP Top 10 for LLM Applications    | 1.1 (2023-10)           | LLM-specific vulnerability taxonomy          |
