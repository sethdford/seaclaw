---
title: "Memory & Cognition Systems — arXiv Research Synthesis"
created: 2026-04-03
status: active
scope: episodic memory, semantic memory, working memory, knowledge graphs, cognitive architectures
papers: 8
---

# Memory & Cognition Systems — arXiv Research Synthesis

State-of-the-art research on memory architectures for AI agents (2025-2026), with focus on conversational memory, episodic/semantic separation, and cognitive-science-inspired designs.

---

## Survey Papers (Start Here)

### 1. Memory in the Age of AI Agents (December 2025)

**Paper:** [arXiv:2512.13564](https://arxiv.org/abs/2512.13564)
**Title:** "Memory in the Age of AI Agents: A Survey"
**Paper list:** [GitHub](https://github.com/Shichun-Liu/Agent-Memory-Paper-List)

Comprehensive survey covering memory as a core capability of foundation model-based agents. Organizes the field into:
- **Encoding** — how experiences become memories
- **Storage** — where and how memories persist
- **Retrieval** — how memories are recalled
- **Application** — how memories influence behavior

**Key Taxonomy:**
| Memory Type | What It Stores | h-uman Equivalent |
| --- | --- | --- |
| Working Memory | Current context window | Conversation context / awareness builder |
| Episodic Memory | Specific past interactions | `channel_history`, `mood_log` |
| Semantic Memory | Structured factual knowledge | `contact_baselines`, `opinions`, Knowledge Graph |
| Procedural Memory | Learned skills/patterns | Tool preferences, HuLa programs |

---

### 2. AI Meets Brain (December 2025)

**Paper:** [arXiv:2512.23343](https://arxiv.org/abs/2512.23343)
**Title:** "AI Meets Brain: A Unified Survey on Memory Systems from Cognitive Neuroscience to Autonomous Agents"

**Key Insight:** Bridges cognitive neuroscience memory models with AI agent implementations. Argues that successful agent memory should mirror the brain's consolidation process: experience → encoding → consolidation (during "sleep"/idle) → long-term storage.

**Implications for h-uman:**
- Our proactive check cycle (hourly with jitter) could serve as a "consolidation" phase
- Memory promotion from working → episodic → semantic should happen during idle, not in real-time conversation
- Aligns with Phase 6's `life_chapters` as consolidated narrative memory

---

### 3. From Human Memory to AI Memory (April 2025)

**Paper:** [arXiv:2504.15965](https://arxiv.org/abs/2504.15965)
**Title:** "From Human Memory to AI Memory: A Survey on Memory Mechanisms in the Era of LLMs"

Comprehensive mapping of human memory mechanisms to LLM implementations. Covers encoding specificity, retrieval cues, interference effects, and forgetting curves.

---

## Architectural Papers

### 4. Memoria: Scalable Agentic Memory (December 2025)

**Paper:** [arXiv:2512.12686](https://arxiv.org/abs/2512.12686)
**Title:** "Memoria: A Scalable Agentic Memory Framework for Personalized Conversational AI"

**Architecture:**
- **Episodic Memory** — recalls specific past interactions, user preferences, prior conversations, historical decisions (autobiographical)
- **Semantic Memory** — structured factual knowledge: historical data, domain definitions, taxonomic relationships
- Clear separation enables different retrieval strategies per memory type

**Implications for h-uman:**
- Validates our multi-engine memory approach
- Need explicit separation in retrieval: "what happened" (episodic) vs "what I know" (semantic)
- Our `sqlite` engine blends these — consider separate retrieval paths

---

### 5. Multi-Layered Memory Architectures (March 2026)

**Paper:** [arXiv:2603.29194](https://arxiv.org/abs/2603.29194)
**Title:** "Multi-Layered Memory Architectures for LLM Agents: An Experimental Evaluation of Long-Term Context Retention"

**Architecture:**
| Layer | Function | TTL | h-uman Mapping |
| --- | --- | --- | --- |
| Working Memory | Recent interaction within bounded windows | Session | Conversation context |
| Episodic Memory | Compact session summaries | Days-weeks | `channel_history` summaries |
| Semantic Memory | Structured entity-level abstractions | Persistent | Knowledge graph, `contact_baselines` |

**Key Innovation:** **Adaptive layer-weighting mechanism** — controls retrieval importance dynamically based on query type. Reduces false memory rate and decreases context usage.

**Implications for h-uman:**
- **Critical gap:** We have the layers but no adaptive weighting for retrieval
- Need a query classifier: "is this asking about a specific event (episodic) or general knowledge (semantic)?"
- False memory reduction is important for the "close friend test" — humans tolerate fuzzy recall but not fabricated memories
- Adaptive weighting should account for recency, salience, and emotional significance

---

### 6. Simple Yet Strong Baseline for Long-Term Conversational Memory (November 2025)

**Paper:** [arXiv:2511.17208](https://arxiv.org/abs/2511.17208)
**Title:** "A Simple Yet Strong Baseline for Long-Term Conversational Memory of LLM Agents"

**Key Finding:** Simple approaches (structured summarization + retrieval) outperform complex architectures on real conversational data. Over-engineering memory is a common failure mode.

**Implications for h-uman:**
- Before building complex graph traversal, ensure simple baselines are solid
- Our existing SQLite + summary approach may already be competitive
- Eval should compare simple vs complex memory retrieval paths

---

### 7. Multiple Memory Systems for Long-Term Agent Memory (August 2025)

**Paper:** [arXiv:2508.15294](https://arxiv.org/abs/2508.15294)
**Title:** "Multiple Memory Systems for Enhancing the Long-term Memory of Agent"

Cognitive science-inspired multiple memory systems with distinct encoding, consolidation, and retrieval processes per memory type.

---

### 8. Cognitive Architecture of Symbolic Identity (ScienceDirect, 2026)

**Source:** [ScienceDirect](https://www.sciencedirect.com/science/article/abs/pii/S1389041726000343)
**Title:** "The cognitive architecture of symbolic identity: Structuring coherence in human-AI reasoning systems"

**Key Concept:** BALLERINA architecture — containment architecture that stabilizes AI agent reasoning, behavior, and identity across variable interaction conditions.

**Implications for h-uman:**
- Identity stability is the foundation for personality consistency (scored 6/10)
- Our persona system provides identity, but needs **containment** — resistance to prompt injection, role drift, and contextual identity loss
- Aligns with Phase 6's self-awareness features

---

## Gap Analysis: h-uman Memory vs SOTA

| Capability | SOTA (2026 papers) | h-uman Current | Gap |
| --- | --- | --- | --- |
| **Memory layers** | Working/Episodic/Semantic/Procedural | Multi-engine (SQLite, recall, core) | Partially aligned — layers exist but not explicitly typed |
| **Adaptive retrieval weighting** | Dynamic per-query type | Static retrieval | **Major gap** — no query-type-aware weighting |
| **Consolidation during idle** | Explicit consolidation phase | Proactive check cycle | Partially aligned — need explicit memory consolidation step |
| **Episodic vs semantic separation** | Distinct retrieval paths | Blended in SQLite | **Gap** — need separate retrieval strategies |
| **False memory prevention** | Active detection and filtering | No explicit mechanism | **Gap** — especially important for "close friend test" |
| **Forgetting/decay** | Principled forgetting curves | No decay mechanism | **Gap** — old memories weighted same as recent |
| **Identity containment** | BALLERINA-style stability | Persona system | Partially aligned — need drift resistance |
| **Simple baselines first** | Proven to outperform complex systems | Complex multi-engine | **Validation gap** — are simple paths competitive? |

---

## Recommended Reading Order

1. Memory in the Age of AI Agents (survey — big picture)
2. Multi-Layered Memory Architectures (closest to our architecture)
3. AI Meets Brain (cognitive neuroscience mapping)
4. Simple Yet Strong Baseline (reality check on complexity)
5. Memoria (episodic/semantic separation)
6. Cognitive Architecture of Symbolic Identity (identity stability)
