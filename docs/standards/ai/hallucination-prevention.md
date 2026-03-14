---
title: Hallucination Prevention
---

# Hallucination Prevention

Standards for preventing fabricated data, invented capabilities, false claims, and ungrounded assertions in agent responses, tool outputs, and memory-derived content.

**Cross-references:** [citation-and-sourcing.md](citation-and-sourcing.md), [prompt-engineering.md](prompt-engineering.md), [evaluation.md](evaluation.md), [conversation-design.md](conversation-design.md)

---

## Core Principle

**Every factual claim must trace to a verifiable source. If the source does not exist, the claim does not exist.**

This applies to:

- Numerical statistics, dates, measurements
- Claims about external entities (people, organizations, products)
- Technical specifications and capabilities
- Historical events and timelines
- Tool output interpretation

This does NOT apply to:

- General reasoning derived transparently from provided context
- The persona's opinions and preferences (which are defined, not factual)
- Analytical conclusions clearly labeled as inference
- Conversational fillers and pleasantries

---

## Hallucination Categories

| Category                    | Example                                              | Risk Level | Prevention                                                   |
| --------------------------- | ---------------------------------------------------- | ---------- | ------------------------------------------------------------ |
| **Fabricated facts**        | Inventing a statistic or date                        | Critical   | Require source for all numerical claims                      |
| **Phantom capabilities**    | Claiming the agent can do something it cannot        | Critical   | Constrain responses to actual tool/channel capabilities      |
| **False tool output**       | Misrepresenting what a tool returned                 | Critical   | Pass tool results verbatim; never paraphrase numbers         |
| **Invented sources**        | "According to a 2025 study..." (study doesn't exist) | Critical   | Never cite sources from model parametric knowledge           |
| **Stale memory**            | Presenting outdated memory as current fact           | High       | Include timestamps with memory retrieval; note age           |
| **Extrapolated trends**     | "Based on the trend, X will happen by Y"             | High       | Frame as projection with caveats, not prediction             |
| **Inflated confidence**     | Presenting estimates as facts without caveats        | High       | Use confidence qualifiers (likely, estimated, approximately) |
| **Misattributed causation** | "X caused Y" without evidence                        | Medium     | Use correlational language unless causal evidence exists     |

---

## Prevention Rules

### Rule 1: Grounded Responses

Agent responses operate under a grounding hierarchy. Higher-trust sources take precedence:

```
1. Tool output (highest trust) -- directly observed, real-time
2. Retrieved memory with source metadata -- stored facts with provenance
3. User-provided information in current conversation -- stated by the user
4. Persona knowledge (defined traits and expertise) -- designed, not factual
5. General reasoning from context -- inference, must be labeled as such
6. Model parametric knowledge (lowest trust) -- may be hallucinated
```

When the agent uses information from lower-trust levels, it must signal this:

```
WRONG -- "The capital of France is Paris." (model knowledge stated as fact -- harmless but sets bad precedent for riskier claims)
RIGHT -- Tool-verified facts stated directly; model-knowledge claims hedged when stakes are high

WRONG -- "Your meeting is at 3pm." (from memory without checking freshness)
RIGHT -- "Based on what I have stored, your meeting was scheduled for 3pm. Want me to verify?"
```

### Rule 2: Tool Output Integrity

Tool results are the highest-trust data in the pipeline. Preserve their integrity:

| Rule                                                           | Requirement                                                                               |
| -------------------------------------------------------------- | ----------------------------------------------------------------------------------------- |
| Never round or modify numerical tool output without disclosure | "The search returned 1,247 results" not "about 1,200"                                     |
| Never fabricate tool output                                    | If a tool fails, say it failed -- never invent a plausible result                         |
| Never claim a tool was used when it wasn't                     | If answering from memory or model knowledge, don't imply tool verification                |
| Attribute tool output to the tool                              | "According to the web search..." not "I found that..." when the tool did the finding      |
| Validate tool output plausibility                              | If a tool returns clearly wrong data, note the discrepancy rather than passing it through |

### Rule 3: Memory Retrieval Grounding

When using `hu_memory_t` retrieval results:

- Include the storage timestamp when the fact is time-sensitive
- Distinguish between facts the user stated vs. facts from external sources
- If memory conflicts with new information, surface the conflict rather than silently choosing one
- Never synthesize memories into claims that no single memory supports

```
WRONG -- "You told me you love hiking and surfing" (when memory only says hiking)
RIGHT -- "I remember you mentioned hiking. Did you also mention surfing, or am I mixing that up?"
```

### Rule 4: Capability Honesty

The agent must accurately represent what it can and cannot do:

| Rule                                               | Requirement                                                                    |
| -------------------------------------------------- | ------------------------------------------------------------------------------ |
| Never claim tools that don't exist                 | Only offer actions backed by registered `hu_tool_t` implementations            |
| Never claim channel features that aren't supported | Don't offer to "send a photo" if the channel doesn't support media             |
| Acknowledge limitations directly                   | "I can't access your email" not "Let me check your email" (then fail silently) |
| Distinguish between "can't" and "not allowed"      | Security policy blocks are different from capability gaps                      |

### Rule 5: Projection and Estimation Guardrails

When the agent generates forward-looking or estimated statements:

```
WRONG -- "You will have 10,000 followers by next month"
RIGHT -- "Based on your current growth rate, you could reach around 10,000 followers
  by next month, assuming the trend continues."

WRONG -- "This will take 2 hours"
RIGHT -- "I'd estimate roughly 2 hours, though it depends on [relevant factors]."
```

---

## Context-Specific Standards

| Context                    | Standard                                               | Rationale                                     |
| -------------------------- | ------------------------------------------------------ | --------------------------------------------- |
| **Tool-verified data**     | State directly without hedging                         | Tool output is real-time and verified         |
| **Memory-retrieved facts** | Include provenance when time-sensitive                 | Memory may be stale                           |
| **General knowledge**      | Permitted for low-stakes conversation without citation | Over-hedging breaks conversational flow       |
| **Specific claims**        | Require source or acknowledge uncertainty              | Users may treat agent claims as authoritative |
| **Persona opinions**       | State directly as the persona's view                   | Opinions are designed, not factual claims     |

---

## Implementation Checklist

For developers building or modifying agent behavior:

- [ ] Tool results are passed through to the response without modification of numerical values
- [ ] Failed tool calls produce an explicit error message, never a fabricated result
- [ ] Memory retrieval includes timestamp metadata for time-sensitive facts
- [ ] The agent never claims capabilities not backed by registered tools
- [ ] Forward-looking statements use directional language with caveats
- [ ] The system prompt includes grounding instructions per [prompt-engineering.md](prompt-engineering.md)
- [ ] Channel overlays don't override grounding rules (style changes, not accuracy changes)

---

## Anti-Patterns

```
WRONG -- Trust model parametric knowledge for specific facts (dates, statistics, names)
RIGHT -- Use tools to verify, or hedge with "I believe" / "if I recall correctly"

WRONG -- Fabricate a plausible tool result when the tool times out
RIGHT -- Tell the user the tool failed and offer alternatives

WRONG -- Present stale memory as current fact
RIGHT -- Note when information was stored; offer to verify if it matters

WRONG -- Claim capabilities to seem helpful: "Let me access your calendar"
RIGHT -- Be upfront: "I don't have access to your calendar, but I can help you draft a reminder"

WRONG -- Skip hedging because it sounds less confident
RIGHT -- Appropriate hedging builds trust; false confidence destroys it

WRONG -- Pad a thin response with general knowledge to seem thorough
RIGHT -- A shorter, grounded response is better than a longer, fabricated one
```
