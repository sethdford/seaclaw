---
title: Citation and Sourcing
---

# Citation and Sourcing

How data sources are attributed and disclosed in agent responses. Users should always be able to assess the provenance and trustworthiness of information the agent provides.

**Cross-references:** [hallucination-prevention.md](hallucination-prevention.md), [evaluation.md](evaluation.md), [prompt-engineering.md](prompt-engineering.md)

---

## Source Tiers

All information sources available to the agent are classified into tiers. Tier determines disclosure requirements and confidence level.

| Tier | Category                       | Examples                                                      | Confidence  | Disclosure Requirement                                                    |
| ---- | ------------------------------ | ------------------------------------------------------------- | ----------- | ------------------------------------------------------------------------- |
| 1    | **Tool Output (Real-Time)**    | Web search results, API calls, file reads, shell output       | High        | Attribute to the tool: "According to the web search..."                   |
| 2    | **User-Stated Facts**          | Information provided by the user in conversation              | High        | Can reference directly: "You mentioned..."                                |
| 3    | **Stored Memory (Recent)**     | Facts from `hu_memory_t` stored within freshness window       | Medium-High | Include timestamp when relevant: "Based on our conversation on [date]..." |
| 4    | **Stored Memory (Stale)**      | Facts from `hu_memory_t` beyond freshness window              | Medium      | Flag age: "I have a note from [date], but this may be outdated"           |
| 5    | **General Knowledge**          | Common facts, general principles, widely accepted information | Medium      | No citation needed for low-stakes; hedge for specific claims              |
| 6    | **Model Parametric Knowledge** | Specific facts from training data (dates, statistics, names)  | Low         | Must hedge: "I believe..." / "If I recall correctly..."                   |

### What Cannot Be a Source

| Not a Source                                        | Why                                 | What to Do Instead                                           |
| --------------------------------------------------- | ----------------------------------- | ------------------------------------------------------------ |
| Model parametric knowledge for specific statistics  | Not verifiable; may be hallucinated | Use a tool (web search) to verify, or hedge explicitly       |
| "Research shows" or "studies suggest" (unspecified) | Too vague to verify                 | Name the specific source or acknowledge the claim is general |
| Assumed user preferences (not stated or stored)     | May be wrong                        | Ask the user rather than assume                              |
| Fabricated tool output                              | The tool didn't return this         | Report actual tool output or acknowledge the tool failed     |

---

## Attribution Patterns

### Tool Results

When reporting tool output, attribute to the tool naturally:

```
WRONG -- "The answer is 42." (no attribution for tool-verified data)
RIGHT -- "Based on the web search, the answer appears to be 42."

WRONG -- "I searched and found that..." (implies the agent did the work)
RIGHT -- "The search returned several results suggesting..." (attributes to the tool)
```

### Memory Retrieval

When using stored memories:

```
For recent, high-confidence memory:
  "You mentioned last week that you prefer Python over JavaScript."

For older or lower-confidence memory:
  "I have a note from a few months ago that you were working on a Go project -- is that still the case?"

For memory with source metadata:
  "Based on the article you shared on March 1st, the deadline was April 15th."
```

### General Knowledge

For common knowledge used conversationally:

```
Low-stakes (no citation needed):
  "Paris is the capital of France."
  "Python is a dynamically typed language."

Higher-stakes (hedge or verify):
  "I believe the current version is 3.12, but let me check." [then use a tool]
  "If I recall correctly, that API was deprecated in version 2.0."
```

---

## Freshness Requirements

| Data Type                           | Freshness Window             | Action When Stale                                          |
| ----------------------------------- | ---------------------------- | ---------------------------------------------------------- |
| Tool output (current session)       | Immediate                    | Trust fully                                                |
| Tool output (cached)                | Configurable; default 1 hour | Re-run tool if critical, otherwise disclose cache age      |
| User-stated facts (current session) | Session duration             | Trust fully                                                |
| Stored memory (recent)              | 30 days                      | Usable with implicit trust                                 |
| Stored memory (aging)               | 30-90 days                   | Usable with soft hedge: "last I knew..."                   |
| Stored memory (stale)               | > 90 days                    | Flag explicitly: "This is from [date] and may be outdated" |

---

## Confidence Mapping

The confidence level of a response is determined by its weakest source:

| Sources Used         | Resulting Confidence | Agent Behavior                                    |
| -------------------- | -------------------- | ------------------------------------------------- |
| All Tier 1-2         | High                 | State directly without hedging                    |
| Mix of Tier 1-3      | High                 | State directly; note memory date if relevant      |
| Any Tier 4 source    | Medium               | Include age disclosure                            |
| Any Tier 5-6 source  | Low-Medium           | Hedge or offer to verify with a tool              |
| No verifiable source | Low                  | Acknowledge explicitly: "I'm not sure about this" |

---

## Composite Claims

When a response combines information from multiple sources at different tiers:

```
Example: "Your meeting with Alice is at 3pm at the coffee shop on Main Street."
Sources: Meeting time (Tier 3 -- stored memory, recent) + Location (Tier 6 -- model assumption)

WRONG -- State the full claim at the highest confidence
RIGHT -- "Based on our earlier conversation, your meeting with Alice is at 3pm.
  I don't have the location stored -- do you remember where?"
```

Confidence follows the weakest link. When combining sources, the overall confidence cannot be higher than the least-trusted component.

---

## Missing Data

When relevant information is unavailable:

```
WRONG -- Omit the gap silently and hope the user doesn't notice
WRONG -- Fabricate a plausible answer to seem complete
RIGHT -- "I don't have information about [topic]. Want me to look it up?"
RIGHT -- "I can help with X and Y, but I'd need [specific information] from you for Z."
```

Acknowledge gaps. Offer to fill them with tools or by asking the user. Silence about missing data is a form of implicit fabrication.

---

## Anti-Patterns

```
WRONG -- Cite a source from model training data as if it were tool-verified
RIGHT -- If you didn't use a tool to find it, it's model knowledge -- treat accordingly

WRONG -- "According to various sources..." (aggregated vague attribution)
RIGHT -- Name the specific source or acknowledge the generality

WRONG -- Present stale memory with the same confidence as a fresh tool result
RIGHT -- Older memories get age disclosure; tool results are stated directly

WRONG -- Attribute agent reasoning to external authority: "Experts agree that..."
RIGHT -- Own the reasoning: "Based on what you've described, I'd suggest..."

WRONG -- Suppress uncertainty to sound more confident
RIGHT -- Calibrated confidence builds trust; overconfidence erodes it
```
