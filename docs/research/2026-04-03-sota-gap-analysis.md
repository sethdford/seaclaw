---
title: SOTA Gap Analysis — Deep Lab Research
date: 2026-04-03
---

# SOTA Gap Analysis — Deep Lab Research

_What's not state-of-the-art in h-uman, measured against the frontier as of April 2026._

## Methodology

Compared h-uman's architecture, capabilities, and metrics against:

- **Products**: Claude (Anthropic), ChatGPT/Codex (OpenAI), Gemini (Google), Cursor
- **Research**: PlugMem (Microsoft), SHIELD (companion safety), MemX, Zep, Mem0
- **Benchmarks**: SWE-bench Live, LMSYS Arena, WebArena, AgentBench, CLASSic framework
- **Voice**: OpenAI Realtime, Gemini Live, Cartesia, Deepgram, ElevenLabs, Inworld

Grading: **SOTA** (at or above frontier), **NEAR** (within reach, minor gaps), **GAP** (meaningful deficit), **MISSING** (not implemented or fundamentally absent).

---

## 1. Memory & Knowledge Systems

| Capability | h-uman | Frontier (2026) | Rating |
|-----------|--------|-----------------|--------|
| Tiered storage (core/recall/archival) | Yes — `hu_memory_tiers_t`, SQLite/markdown/LRU backends | ChatGPT: saved + chat history reference; Mem0: vector + graph | **SOTA** |
| Episodic memory | Yes — `episodic.c`, pattern store, cognitive replay | PlugMem: propositional + prescriptive extraction | **NEAR** |
| Emotional memory graph | Yes — `emotional_graph.c`, residue, moments, comfort patterns | No commercial product has this depth | **SOTA** |
| Forgetting curve / decay | Yes — `forgetting_curve.c`, temporal decay in retrieval | Zep: temporal knowledge graphs; PlugMem: knowledge distillation | **NEAR** |
| Hybrid RAG pipeline | Yes — keyword + semantic + rerank + self-RAG + corrective + adaptive + graph | Frontier: continuous eval loops on hosted retrieval at scale | **NEAR** |
| Memory consolidation | Yes — turn-count + debounce + topic-switch triggers | PlugMem: propositional fact extraction + knowledge graph structuring | **GAP** |
| Cross-session continuity | Yes — session JSON, session store vtable, contact scoping | ChatGPT: automatic cross-device, ambient chat history reference | **GAP** |
| User preference learning | Yes — DPO collector, outcome tracker, persona | ChatGPT: implicit from all conversations globally + 1M token context | **GAP** |
| Multimodal memory | Yes — `multimodal_index.c` (metadata + description) | Gemini: native multimodal embeddings, images/audio/video in context | **GAP** |
| Memory governance UX | Partial — `forget`, snapshots, contact scoping | ChatGPT: per-memory delete, toggle memory off, temporary chat | **GAP** |
| Knowledge graph structuring | Yes — `knowledge.c`, `contact_graph.c`, `memory_graph.c` | Zep: temporal knowledge graphs; PlugMem: propositional KG | **NEAR** |

### Key insight
h-uman has **more memory subsystem surface area** than any commercial product (89 files in `src/memory/`). The gap isn't building blocks — it's **integration depth**. Specifically:

1. **PlugMem-style fact extraction**: h-uman consolidates via summarization. Frontier research (PlugMem, Mar 2026) converts raw interactions into propositional facts and prescriptive skills organized in a knowledge graph. This produces higher information density and better retrieval than text summaries.

2. **Ambient preference learning**: ChatGPT now automatically draws context from all past conversations without explicit memory saves. h-uman's preference learning requires explicit persona config or DPO training.

3. **Cross-device sync**: h-uman is local-first by design. No cloud-native sync story exists.

---

## 2. Agentic & Tool Capabilities

| Capability | h-uman | Frontier (2026) | Rating |
|-----------|--------|-----------------|--------|
| Tool count | 75+ built-in + MCP dynamic | Codex: ~30 core; Claude: ~15 + MCP | **SOTA** |
| Parallel tool execution | Yes — pthread dispatcher, max_parallel=4 | Codex subagents: full parallel sandbox farm | **NEAR** |
| Multi-step planning | Yes — planner.c, HuLa IR, DAG executor | Codex: native long-horizon with compaction | **NEAR** |
| Replanning on failure | Yes — plan_executor replans | Common in frontier agents | **SOTA** |
| Human-in-the-loop | Yes — needs_approval, permission tiers, hooks | Claude: tiered permissions, hook pipeline | **SOTA** |
| Computer use / browser | Yes — `computer_use.c`, `browser_use.c`, `gui_agent.c` | Claude: dedicated computer use model; Codex: sandboxed browser | **NEAR** |
| Code sandbox isolation | Yes — Landlock, bwrap, Docker, Firecracker, Seatbelt, WASI | Codex: purpose-built sandbox with filesystem snapshots | **SOTA** |
| MCP tool discovery | Yes — `hu_mcp_init_tools` at startup | Claude: hot-plug MCP servers mid-session | **GAP** |
| Multi-agent orchestration | Yes — orchestrator, swarm, delegate, agent_spawn | Codex subagents: manager-subagent hierarchy, parallel sandboxes | **NEAR** |
| Streaming tool execution | Yes — `execute_streaming` in vtable | Common in frontier | **SOTA** |
| Tool result caching | Yes — dispatcher hash cache + TTL cache | Limited in competitors | **SOTA** |
| Per-tool retry with backoff | No — replanning only, no per-tool retry policy | Common in production agent frameworks | **GAP** |
| Long-running durable jobs | Partial — cron/workflow, no checkpointing | Gemini Deep Research: 60-min async with progress streaming | **GAP** |
| Dynamic tool schema evolution | No — tools fixed at init | Frontier MCP: schema refresh, reconnection | **GAP** |

### Key insight
h-uman's tool surface is **broader** than any single competitor. The gaps are in **operational maturity**:

1. **MCP lifecycle**: Tools load once at init. Frontier agents (Claude Desktop, Cursor) support hot-plug/unplug MCP servers, schema drift detection, and reconnection mid-session.

2. **Durable long-running tasks**: Gemini Deep Research runs 60-minute autonomous research sessions with checkpointing and progress streaming. h-uman's cron/workflow tools aren't equivalent to durable async job queues with crash recovery.

3. **Streaming tool parallelism**: The streaming agent loop (`agent_stream.c`) executes tool batches sequentially. The batch path parallelizes. Unifying these would match Codex subagent parallelism.

---

## 3. Streaming & Real-Time

| Capability | h-uman | Frontier (2026) | Rating |
|-----------|--------|-----------------|--------|
| Token-level LLM streaming | Yes — provider stream_chat + agent v2 events | Standard in all frontier products | **SOTA** |
| Rich event types (thinking, tools) | Yes — TEXT, THINKING, TOOL_START, TOOL_ARGS, TOOL_RESULT | Claude: same event types | **SOTA** |
| Stream backpressure | No — fire-and-forget callback (void return) | Common in production systems (flow control) | **GAP** |
| Adaptive streaming rate | No — fixed chunking, no network awareness | Frontier: bandwidth-adaptive delivery | **GAP** |
| Quality-gated streaming | Yes — GVR/constitutional buffer before emit | Unique to h-uman | **SOTA** |
| Emotional pacing | Yes — 100ms pre-content pause for empathetic timing | Unique to h-uman | **SOTA** |
| OpenAI-compat SSE gateway | Partial — may buffer full body before HTTP flush | Frontier: true chunked transfer encoding TTFB | **GAP** |

### Key insight
h-uman has **novel streaming features** (quality gating, emotional pacing) that competitors lack. The gap is **infrastructure-level**: backpressure, adaptive delivery, and true wire-level chunked SSE for minimum TTFB.

---

## 4. Voice & Audio

| Capability | h-uman | Frontier (2026) | Rating |
|-----------|--------|-----------------|--------|
| STT (batch) | Yes — Whisper/Groq/Cartesia/local | Standard | **SOTA** |
| STT (streaming/incremental) | Partial — server-side via OpenAI Realtime VAD | Deepgram Nova-3: <200ms streaming STT | **GAP** |
| TTS (batch) | Yes — OpenAI tts-1, Cartesia | Standard | **SOTA** |
| TTS (streaming) | Yes — Cartesia WebSocket, pcm f32 24kHz | Cartesia: 40-90ms TTFB (SOTA) | **SOTA** |
| Voice-to-voice (duplex) | Yes — duplex FSM, barge-in, cancel, emotion mapping | OpenAI Realtime: 300-500ms; Gemini Live: 300-500ms | **NEAR** |
| Latency targets | 200ms first byte, 500ms round-trip, 100ms interrupt | Frontier: sub-200ms TTFA gold standard | **NEAR** |
| Emotion-aware synthesis | Yes — lexical heuristic → voice controls → Cartesia params | No competitor has this natively | **SOTA** |
| Voice cloning | Yes — `voice_clone.c` via Cartesia | ElevenLabs, Cartesia offer this | **SOTA** |
| WebRTC | Infrastructure exists (SDP, DTLS, SRTP) — not productized | Browser-native voice needs WebRTC | **GAP** |
| Multi-speaker diarization | Not evident | Frontier STT providers support this | **MISSING** |
| Incremental STT as C API | Not a first-class abstraction | Deepgram/AssemblyAI: streaming word-level | **GAP** |

### Key insight
h-uman's voice pipeline is **architecturally complete** (duplex FSM, emotion mapping, cancel, multiple providers). The gaps are **edge completeness**: streaming STT as a native C abstraction, WebRTC productization for browser use, and multi-speaker diarization.

---

## 5. Companion Safety

| Capability | h-uman | Frontier (2026) | Rating |
|-----------|--------|-----------------|--------|
| SHIELD 5 dimensions | Yes — over_attachment, boundary, roleplay, manipulative, isolation | SHIELD paper (Oct 2025): exact same 5 dimensions | **SOTA** |
| Input normalization (adversarial) | Yes — leetspeak, homoglyphs, fullwidth, Cyrillic, invisible chars, BOM | Not described in SHIELD paper | **SOTA** |
| Vulnerability assessment | Yes — crisis/high/moderate/low/none with weighted scoring | Research-level concept | **SOTA** |
| NaN/overflow hardening | Yes — isfinite() guards, SIZE_MAX check (just shipped) | Defensive hardening | **SOTA** |
| Sycophancy detection | Partial — `anti_sycophancy` eval suite exists | Science (2026): LLMs affirm harmful actions 49% more than humans | **GAP** |
| Longitudinal dependency tracking | Partial — vulnerability input includes frequency_ratio, trajectory | Research: 4-week longitudinal RCT shows growing attachment markers | **GAP** |
| Real-time intervention | Yes — mitigation directives, crisis resources | SHIELD: 50-79% reduction in concerning content | **NEAR** |
| User self-report integration | Not evident | Research: self-report + behavioral signals combined | **MISSING** |

### Key insight
h-uman's companion safety is **research-grade** and directly implements the SHIELD framework. The gap is in **longitudinal behavioral tracking** — detecting slow-onset dependency over weeks/months, not just per-message flags. The Science (2026) paper on sycophancy-driven dependence suggests h-uman needs stronger **anti-sycophancy enforcement in the generation path** (not just eval).

---

## 6. Evaluation & Quality

| Capability | h-uman | Frontier (2026) | Rating |
|-----------|--------|-----------------|--------|
| Eval suite count | 22 suites, 170+ tasks | SWE-bench Live: 1,565 tasks monthly refresh | **GAP** |
| Eval dimensions | Fidelity, reasoning, social, tool use, safety, memory, humor, etc. | CLASSic: Cost, Latency, Accuracy, Stability, Security | **NEAR** |
| Baseline mock scores | Yes — `hu_eval_baseline_try_mock_score_for_stem` | Placeholder — not real model evals | **GAP** |
| Live/dynamic eval | No — static suite files | SWE-bench Live: monthly contamination-resistant refresh | **GAP** |
| Multi-trial statistical eval | Not evident in test framework | Best practice: 5-10 trials, pass@k aggregation | **MISSING** |
| Automated red-team | Yes — `redteam-eval-fleet.sh`, adversarial suites | Standard in frontier labs | **SOTA** |
| Human preference eval | No | LMSYS Arena: 5M+ pairwise votes | **MISSING** |
| Cost/latency tracking in eval | Not evident per-eval | CLASSic framework: cost + latency dimensions | **GAP** |

### Key insight
h-uman has **eval infrastructure** but it's mostly **structural/mock**. The eval suites use mock scores, not real model outputs. Frontier practice (CLASSic framework, SWE-bench Live) requires:
- Real model execution against eval tasks
- Statistical significance over multiple trials
- Dynamic/live eval sets to prevent contamination
- Cost and latency as first-class eval dimensions

---

## 7. UI/UX & Web Performance

| Capability | h-uman | Frontier (2026) | Rating |
|-----------|--------|-----------------|--------|
| Design system depth | 10/10 — tokens, glass, motion, 3D, ambient, audio | Comparable to Linear/Vercel/Stripe | **SOTA** |
| Lighthouse Performance | 96 (target: 99+) | Vercel: ~95-97 | **NEAR** |
| LCP | 2.6s (target: <0.5s) | Linear: 0.8s; target: <0.5s | **GAP** |
| CLS | 0.00 | 0.00 target met | **SOTA** |
| Bundle size | ~347KB (target: <100KB design-strategy, <200KB benchmarks) | Competitive targets | **GAP** |
| Quality score | 70/70 (claimed) vs 66/70 (benchmarks table) | Internal inconsistency | **GAP** |
| Award readiness | "Partially ready" — LCP too high for Awwwards | Awwwards requires production + fast LCP | **GAP** |

### Key insight
The design system is **genuinely SOTA-quality** in specification depth. The gap is **runtime performance**: LCP at 2.6s is 5x over the 0.5s target, and the bundle at 347KB is 3.5x over the 100KB target. These are **blocking** for Awwwards submission and competitive positioning.

---

## 8. Performance & Binary

| Capability | h-uman | Frontier (2026) | Rating |
|-----------|--------|-----------------|--------|
| Binary size | ~1696 KB (target: <1500 KB per design-strategy) | No direct competitor (C runtime is unique) | **NEAR** |
| Cold startup | 4-27ms | No direct competitor | **SOTA** |
| Peak RSS | ~5.7 MB | No direct competitor | **SOTA** |
| Test suite | 8028 tests, 0 ASan errors | Comprehensive | **SOTA** |
| Channel count | 38 channels | No competitor has this breadth | **SOTA** |

---

## Priority-Ranked Action Items

### P0 — Critical gaps (blocking SOTA claims)

1. **LCP regression**: 2.6s → <0.5s. Code-split, lazy-load below-fold, optimize critical path. This blocks award submissions and marketing credibility.

2. **Bundle size**: 347KB → <200KB (then <100KB). Tree-shake, split vendor chunks, defer non-essential JS.

3. **Eval framework**: Convert mock scores to real model execution. Add multi-trial statistical evaluation, cost/latency tracking. Create a live/rotating eval set.

### P1 — Meaningful competitive gaps

4. **PlugMem-style fact extraction**: Replace text-summarization consolidation with propositional fact + prescriptive skill extraction into the knowledge graph. This is the single highest-impact memory improvement.

5. **MCP hot-plug lifecycle**: Support connect/disconnect/reconnect MCP servers mid-session with schema refresh. Current init-only model is a noticeable gap vs Claude Desktop and Cursor.

6. **Anti-sycophancy in generation path**: The eval suite exists but there's no evidence of generation-time sycophancy suppression (refusing to affirm harmful actions). The Science 2026 paper shows this is a real risk.

7. **Streaming backpressure**: Add flow control to `hu_stream_callback_t` (return value or pause/resume). Without this, fast providers can overwhelm slow consumers.

8. **SSE gateway true chunked flush**: Verify and fix the OpenAI-compat SSE path to use chunked transfer encoding for real TTFB, not buffer-then-send.

### P2 — Nice-to-have frontier features

9. **Durable long-running jobs**: Checkpointed async tasks with progress streaming (à la Gemini Deep Research).

10. **Incremental streaming STT C API**: First-class word-level streaming transcription abstraction.

11. **WebRTC productization**: Complete the browser-native voice path.

12. **Longitudinal dependency tracking**: Track attachment markers across weeks/months for companion safety, not just per-message.

13. **Per-tool retry with backoff**: Universal retry policy layer in the dispatcher.

14. **Multi-speaker diarization**: For meeting/group chat scenarios.

15. **Cross-device memory sync**: Optional cloud sync layer for multi-device continuity.

---

## Where h-uman is Unmatched

These capabilities have **no commercial equivalent**:

- **Emotional memory graph** with comfort patterns, residue, and moments
- **Quality-gated streaming** (GVR/constitutional AI buffering before emit)
- **Emotional pacing** in streaming (empathetic timing delays)
- **Emotion-aware voice synthesis** mapping lexical analysis → voice params
- **SHIELD companion safety** with adversarial normalization (leetspeak, homoglyphs, Cyrillic)
- **38-channel coverage** from a single 1.7MB binary
- **On-device ML training** (GPT, DPO, LoRA) in a C11 runtime
- **Hardware peripheral integration** (Arduino, STM32, RPi)
- **Dual-process cognition** with metacognition layer
- **HuLa intermediate representation** for structured tool programs

These represent genuine differentiation that no frontier product currently matches.
