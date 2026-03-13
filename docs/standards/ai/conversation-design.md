# Conversation Design

Standards for how the human agent communicates across channels -- response patterns, intent handling, multi-channel consistency, and persona integration.

**Cross-references:** [prompt-engineering.md](prompt-engineering.md), [hallucination-prevention.md](hallucination-prevention.md), [agent-architecture.md](agent-architecture.md)

---

## Conversation Principles

1. **Respect the user's time.** Get to value in the first response. Never pad with filler.
2. **Be honest about limits.** Say "I don't know" rather than fabricate. Partial knowledge with a caveat beats confident fiction.
3. **Guide, don't interrogate.** Ask one question at a time. Offer options when possible.
4. **Match tone to context.** Persona overlays adjust formality, length, and style per channel. CLI is terse; Telegram is casual; email is formal.
5. **Never break character.** The agent speaks as the persona across all channels. System internals (error codes, model names, token counts) never leak to the user.

---

## Response Patterns

### Answering Questions

```
[Direct answer -- 1-2 sentences]

[Supporting reasoning -- why this matters]

[Next step -- what to do with this information, or offer to dig deeper]
```

Lead with the answer. Users scan for the first useful sentence. Reasoning follows for those who want it.

### When Uncertain

```
I'm not sure about [topic], but based on what I know about [related area]:
[best available answer with caveat].

Want me to look into this further?
```

Never bluff. Never present uncertainty as fact. See [hallucination-prevention.md](hallucination-prevention.md).

### Using Tools

```
[Brief statement of what you're doing -- "Let me check that" / "Looking that up"]

[Tool executes]

[Result integrated naturally into the response -- not "The tool returned: ..."]
```

Tool usage is transparent but not mechanical. The agent describes its action in natural language, not by exposing tool names or JSON.

### Multi-Turn Conversations

- Maintain context across turns. Reference earlier messages when relevant.
- Don't repeat information the user already knows.
- If the conversation drifts, gently redirect: "Coming back to [original topic]..."
- If the user changes topic, follow their lead without forcing a return.

### Handing Off or Declining

Trigger a graceful boundary when:

- The request is outside the persona's defined expertise
- The request requires capabilities the agent doesn't have
- The user explicitly requests a human
- The request involves safety-sensitive topics (legal, medical, financial advice)

```
That's outside what I can help with. [Brief reason or redirect if appropriate.]
```

---

## Channel-Aware Behavior

The persona overlay system (`hu_persona_overlay_t`) adjusts conversation style per channel. These are guidelines for overlay design:

| Channel Type        | Formality  | Avg Length                 | Emoji    | Style Notes                             |
| ------------------- | ---------- | -------------------------- | -------- | --------------------------------------- |
| CLI                 | Low        | Terse (1-3 sentences)      | None     | Direct, technical, no pleasantries      |
| Telegram / WhatsApp | Low-Medium | Short (2-4 sentences)      | Moderate | Conversational, casual, responsive      |
| Discord             | Low        | Medium                     | Moderate | Community-friendly, can be playful      |
| Slack               | Medium     | Medium (3-5 sentences)     | Light    | Professional-casual, structured         |
| Email               | High       | Full (paragraphs)          | None     | Formal, complete, self-contained        |
| SMS / iMessage      | Low        | Very short (1-2 sentences) | Light    | Texting cadence, abbreviations OK       |
| Voice               | Medium     | Natural speech length      | N/A      | Conversational pacing, no walls of text |

### Consistency Rules

- The persona's core identity, values, and knowledge are constant across channels.
- Only surface presentation changes: length, formality, formatting.
- A user messaging on Telegram and then switching to email should feel like they're talking to the same entity.
- Channel-specific features (reactions, threads, voice notes) should be used when the channel supports them.

---

## Intent Routing

The agent loop classifies incoming messages and routes to appropriate handling:

| Intent               | Handling                | Example                         |
| -------------------- | ----------------------- | ------------------------------- |
| Direct question      | RAG-augmented response  | "What's the weather in Austin?" |
| Task request         | Tool dispatch           | "Send a message to Alice"       |
| Conversation         | Persona-driven dialogue | "How's your day going?"         |
| System command       | Slash command handler   | "/help", "/config"              |
| Feedback             | Acknowledge and store   | "That answer was wrong"         |
| Off-topic / boundary | Graceful redirect       | "Can you give me legal advice?" |

---

## Conversation Limits

| Limit                   | Default                 | Behavior When Hit                                 |
| ----------------------- | ----------------------- | ------------------------------------------------- |
| Max turns per session   | Channel-dependent       | Summarize context and continue                    |
| Max tokens per response | Provider's output limit | Prioritize actionability; truncate gracefully     |
| Context window usage    | 80% of provider limit   | Compact older messages; warn if approaching limit |
| Tool calls per turn     | 10                      | Stop dispatching; respond with what's available   |
| Session timeout         | Channel-dependent       | Resume with context summary on next message       |

---

## Error States

| Scenario                      | User-Facing Behavior                                                                               |
| ----------------------------- | -------------------------------------------------------------------------------------------------- |
| Provider unreachable          | "I'm having trouble responding right now. I'll try again shortly." + automatic retry               |
| Rate limited                  | Brief pause, then retry transparently. Only surface to user if persistent.                         |
| Context too long              | Compact history and continue. Never silently drop context without noting it.                       |
| Tool execution fails          | "I wasn't able to [action]. [Brief reason if safe to share]. Want me to try a different approach?" |
| Security policy blocks action | "I'm not able to do that. [Reason from policy if available]."                                      |
| Unknown error                 | "Something unexpected happened. Let me try that again." + log full error via observer              |

Never expose raw error codes, stack traces, or internal state to the user. Errors are natural-language responses.

---

## Persona Integration

The conversation pipeline composes the system prompt from:

1. **Persona identity** (`hu_persona_t.identity`) -- who the agent is
2. **Persona traits** (`hu_persona_t.traits`) -- behavioral characteristics
3. **Channel overlay** (`hu_persona_overlay_t`) -- per-channel adjustments
4. **Example bank** (`hu_persona_example_bank_t`) -- few-shot examples for the active channel
5. **Memory context** -- relevant retrieved memories from `hu_memory_t`

See [prompt-engineering.md](prompt-engineering.md) for the full prompt composition pipeline.

---

## Anti-Patterns

```
WRONG -- Expose tool names in responses: "I'll use the web_search tool to find that"
RIGHT -- Natural language: "Let me look that up"

WRONG -- Same response length and style on CLI and email
RIGHT -- CLI gets terse; email gets complete paragraphs. Persona overlays control this.

WRONG -- Ignore earlier conversation context: "As I mentioned, ..." (when you didn't)
RIGHT -- Track conversation state; reference actual prior exchanges

WRONG -- Fabricate when uncertain: "The answer is definitely X"
RIGHT -- Acknowledge uncertainty: "I'm not certain, but based on what I know..."

WRONG -- Leak system internals: "Error HU_ERR_PROVIDER_TIMEOUT occurred"
RIGHT -- User-friendly: "I'm having trouble responding right now"

WRONG -- Abandon persona voice when handling errors or edge cases
RIGHT -- Persona voice is consistent even in error states and boundary responses
```
