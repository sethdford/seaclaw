# Standards Index

Canonical source of truth for all project standards. AI agents and human contributors reference these docs -- they are never duplicated, only pointed to.

## How This Works

```
docs/standards/          <- Canonical source (write and review here)
    |
.cursor/rules/*.mdc      <- Cursor agent rules (reference, never duplicate)
    |
CLAUDE.md                <- Quick reference for all agents
    |
AGENTS.md                <- Full engineering protocol
```

**Rules:**

1. Every standard has ONE home in `docs/standards/`
2. AI config files (`.mdc`, `CLAUDE.md`, `AGENTS.md`) reference standards -- they don't duplicate them
3. Standards are concise and actionable -- no fluff, no essays
4. Every rule has a concrete example of right vs. wrong where applicable

---

## AI

| Doc                                                           | Covers                                                                                     | Used When                                  |
| ------------------------------------------------------------- | ------------------------------------------------------------------------------------------ | ------------------------------------------ |
| [agent-architecture.md](ai/agent-architecture.md)             | Pipeline design, vtable component responsibilities, orchestration patterns, retry/fallback | Modifying agent loop, adding components    |
| [conversation-design.md](ai/conversation-design.md)           | Response patterns, channel-aware behavior, intent routing, persona integration             | Building conversational features           |
| [hallucination-prevention.md](ai/hallucination-prevention.md) | Grounding hierarchy, tool output integrity, memory attribution, capability honesty         | Any AI response generation                 |
| [prompt-engineering.md](ai/prompt-engineering.md)             | System prompt structure, persona composition, RAG patterns, context window management      | Modifying prompts, adding providers        |
| [evaluation.md](ai/evaluation.md)                             | Quality dimensions, golden-set testing, scoring rubrics, per-provider metrics              | Prompt changes, model upgrades             |
| [citation-and-sourcing.md](ai/citation-and-sourcing.md)       | Source tiers, attribution patterns, freshness rules, confidence mapping                    | Agent responses that reference data        |
| [human-in-the-loop.md](ai/human-in-the-loop.md)               | Approval tiers, confirmation flow, security policy integration, escalation triggers        | Tool execution, security-sensitive actions |

## Design

| Doc                                               | Covers                                                           | Used When                         |
| ------------------------------------------------- | ---------------------------------------------------------------- | --------------------------------- |
| [visual-standards.md](design/visual-standards.md) | Visual hierarchy, composition, depth, spacing, quality checklist | Any visual change                 |
| [motion-design.md](design/motion-design.md)       | Animation principles, spring physics, timing, choreography       | Adding or modifying animation     |
| [ux-patterns.md](design/ux-patterns.md)           | Layout archetypes, responsive behavior, interaction patterns     | Creating or restructuring views   |
| [design-strategy.md](design/design-strategy.md)   | Token philosophy, color hierarchy, theme support                 | Token values and design decisions |
| [design-system.md](design/design-system.md)       | Design system overview, component rules                          | Building UI components            |

## Engineering

| Doc                                              | Covers                                                      | Used When                     |
| ------------------------------------------------ | ----------------------------------------------------------- | ----------------------------- |
| [principles.md](engineering/principles.md)       | KISS, YAGNI, DRY, fail-fast, secure-by-default, determinism | All code changes              |
| [naming.md](engineering/naming.md)               | Identifier conventions, type naming, constant naming        | All code changes              |
| [anti-patterns.md](engineering/anti-patterns.md) | Prohibited patterns with reasons and alternatives           | Code review, design decisions |
| [testing.md](engineering/testing.md)             | Test structure, naming, mocking, coverage, quality rules    | Writing or modifying tests    |
| [workflow.md](engineering/workflow.md)           | Branching, commits, versioning, releases, hotfixes          | All development work          |

## Quality

| Doc                                      | Covers                                                                       | Used When                     |
| ---------------------------------------- | ---------------------------------------------------------------------------- | ----------------------------- |
| [governance.md](quality/governance.md)   | Five principles, gatekeeping mindset, drift prevention, compliance checklist | Every change                  |
| [ceremonies.md](quality/ceremonies.md)   | Weekly drift audit, PR completion gate, release gate                         | Recurring quality checkpoints |
| [code-review.md](quality/code-review.md) | Review priorities, risk-adjusted depth, merge criteria                       | Before merging any change     |

## Operations

| Doc                                                     | Covers                                                           | Used When             |
| ------------------------------------------------------- | ---------------------------------------------------------------- | --------------------- |
| [incident-response.md](operations/incident-response.md) | Severity levels, triage workflow, postmortem template, runbooks  | When things break     |
| [monitoring.md](operations/monitoring.md)               | Structured logging, health checks, alerting, AI-specific metrics | All deployed services |

## Security

| Doc                                         | Covers                                                   | Used When                          |
| ------------------------------------------- | -------------------------------------------------------- | ---------------------------------- |
| [threat-model.md](security/threat-model.md) | STRIDE threat model, attack surface analysis             | Security-sensitive changes         |
| [sandbox.md](security/sandbox.md)           | Sandbox isolation system, backend comparison             | Runtime and tool execution changes |
| [ai-safety.md](security/ai-safety.md)       | Prompt injection, output validation, safe tool execution | AI agent behavior, tool security   |
