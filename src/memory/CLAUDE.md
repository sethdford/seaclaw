# src/memory/ — Memory Subsystem (91 files)

Manages agent knowledge storage, retrieval, evolution, and lifecycle. Implements vector search, hybrid retrieval, RAG pipelines, emotional memory, knowledge graphs, and spaced-repetition forgetting.

## Core Engines (hu_memory_t vtable)

```
factory.c            Engine registry and creation
engines/
  registry.c         Engine registration and dispatching
  sqlite.c           SQLite-backed persistent memory
  sqlite_fts.c       SQLite + FTS5 full-text search
  sqlite_lucid.c     SQLite + optional lucid CLI backend
  markdown.c         Markdown file memory
  memory_lru.c       In-memory LRU cache (for testing)
  postgres.c         PostgreSQL-backed memory (HU_ENABLE_POSTGRES)
  redis.c            Redis-backed memory (HU_ENABLE_REDIS_ENGINE)
  api.c              Remote API memory backend
  none.c             No-op backend (testing)
```

## RAG Pipelines

```
rag_pipeline.c       Main RAG orchestration (query → retrieval → generation)
adaptive_rag.c       Adaptive RAG with feedback loops
corrective_rag.c     Corrective RAG (detects and fixes retrieval failures)
self_rag.c           Self-reflective RAG with retrieval critique
fact_extract.c       Fact extraction from responses for memory injection
hallucination_guard.c  Detects and prevents hallucinations
verify_claim.c       Claims verification against knowledge base
```

## Retrieval & Search

```
retrieval/
  engine.c           Hybrid retrieval orchestrator
  hybrid.c           Combines vector + keyword results
  keyword.c          BM25 keyword search
  temporal.c         Temporal (time-aware) retrieval
  reranker.c         Result reranking (cross-encoder)
  llm_reranker.c     LLM-based reranking
  qmd.c              Query-Memory Dispatch (routes queries to strategy)
  strategy_learner.c  Learns which retrieval strategy works best
  query_expansion.c   Expands queries with synonyms/related terms
  entropy_gate.c     Entropy-based confidence gating
  rrf.c              Reciprocal Rank Fusion (combines multiple rankers)
  multigraph.c       Multi-graph retrieval (crosses graph types)
  adaptive.c         Adaptive retrieval parameter tuning
```

## Vector Search & Embeddings

```
vector/
  embeddings.c       Embedding generation dispatch (routes to providers)
  embeddings_gemini.c  Gemini API embeddings (Vertex AI)
  embeddings_ollama.c  Ollama local embeddings
  embeddings_voyage.c  Voyage AI embeddings
  embedder_gemini_adapter.c  Adapter for Vertex AI batch embedding
  embedder_local.c   Local embedding model (TensorFlow Lite)
  provider_router.c  Routes embedding requests to best provider
  store.c            Vector store interface and coordination
  store_mem.c        In-memory vector store (for testing)
  store_pgvector.c   pgvector (PostgreSQL) vector store
  store_qdrant.c     Qdrant vector database integration
  chunker.c          Document chunking for embedding (sliding window, semantic)
  circuit_breaker.c  Circuit breaker for embedding provider failures
  math.c             Vector math (cosine similarity, dot product)
  outbox.c           Outbox pattern for async embedding
  cosine.c           Cosine similarity computation
```

## Knowledge Graphs

```
graph.c              Knowledge graph structure and operations
graph_index.c        Graph indexing (MAGMA, spreading activation)
contact_graph.c      Social contact graph
contact_memory.c     Per-contact relationship memory
connections.c        Cross-memory connection discovery
fast_capture.c       Fast entity/relation extraction
knowledge.c          Knowledge base abstraction
```

## Emotional Systems

```
emotional_graph.c       Tracks emotional patterns across conversations
emotional_residue.c     Persists emotional context between sessions
emotional_moments.c     Identifies emotionally significant memories
comfort_patterns.c      Learned comfort/coping patterns
evolved_opinions.c      Tracks how opinions evolve over time
opinions.c             Opinion storage and retrieval
```

## Episodic & Cognitive Memory

```
episodic.c           Episodic memory (time/context specific events)
stm.c                Short-term memory buffer (conversation-scoped)
deep_memory.c        Deep memory retrieval and analysis
deep_extract.c       Extraction from deep memory stores
cognitive.c          Cognitive processing and memory integration
```

## Memory Evolution & Lifecycle

```
consolidation.c      Merges and deduplicates memories
consolidation_engine.c  Background consolidation processing
promotion.c          Promotes important short-term to long-term
compression.c        Compresses verbose memories to summaries
forgetting.c         Spaced-repetition based forgetting
forgetting_curve.c   Ebbinghaus forgetting curve implementation
degradation.c        Gradual memory quality degradation over time

lifecycle/
  cache.c            Semantic cache (avoids redundant retrievals)
  semantic_cache.c   Semantic similarity-based caching
  hygiene.c          Memory cleanup and quality maintenance
  summarizer.c       Memory summarization and abstraction
  snapshot.c         Memory snapshots for recovery
  migrate.c          Migration between engine backends
  rollout.c          Gradual rollout of engine changes
  diagnostics.c      Memory health diagnostics
```

## Memory Ingestion & Organization

```
ingest.c             Main ingestion pipeline (text → memory storage)
inbox.c              Inbox pattern (buffers memories before consolidation)
policy.c             Memory storage policies (retention, privacy, relevance)
tiers.c              Memory tiering (hot/warm/cold storage)
prospective.c        Prospective memory (reminders, intentions)
life_chapters.c      Life chapter/narrative arc tracking
temporal.c           Temporal organization and time-aware queries
mood.c               Mood context for memory formation
util.c               Memory utilities (parsing, formatting)
```

## Testing & Utilities

```
tests/
  test_memory.c              Core memory operations
  test_memory_full.c         Full memory subsystem integration
  test_memory_engines_ext.c  Engine implementations
  test_retrieval.c           Retrieval pipeline
  test_vector.c              Vector operations
  test_vector_full.c         Full vector search integration
  test_vector_stores.c       Vector store backends
  test_qmd.c                 Query-Memory Dispatch
  test_consolidation.c       Consolidation engine
  test_consolidation_engine.c  Background consolidation
  test_rag.c                 RAG pipeline
  test_rag_pipeline.c        RAG integration
  test_episodic.c            Episodic memory
  test_stm.c                 Short-term memory
  test_emotional_graph.c     Emotional graph
  test_emotional_residue.c   Emotional persistence
  test_emotional_moments.c   Significant moments
  test_forgetting_curve.c    Forgetting curve
  test_degradation.c         Memory degradation
  test_memory_degradation.c  Degradation integration
  test_lifecycle.c           Lifecycle operations
  test_deep_memory.c         Deep memory
  test_deep_extract.c        Deep extraction
  test_cognitive.c           Cognitive processing
  test_graph.c               Knowledge graph
  test_fast_capture.c        Entity extraction
  test_promotion.c           Memory promotion
  test_entropy_gate.c        Entropy gating
  test_hallucination_guard.c  Hallucination detection
  test_fact_extract.c        Fact extraction
```

## Vtable Pattern

All engines implement `hu_memory_t` vtable:
```c
store, retrieve, search, delete, update, stat, flush, close,
get_name, get_provider_config, set_policy, get_stats
```

## Key Concepts

- **Engines** — pluggable backends (SQLite, Postgres, Redis, API, etc.)
- **Retrieval** — hybrid search: vector similarity + keyword + reranking + entropy gating
- **QMD** — query router that selects optimal retrieval strategy
- **RAG** — retrieval-augmented generation with hallucination guards
- **Consolidation** — merges duplicates, promotes important memories, compresses
- **Forgetting** — spaced repetition following Ebbinghaus curve
- **Emotional** — tracks mood, emotional residue, significant moments
- **Graphs** — knowledge graphs with entity extraction and spreading activation

## Rules

- All engines implement `hu_memory_t` fully — no partial implementations
- Use `SQLITE_STATIC` (null), never `SQLITE_TRANSIENT`
- Free all allocations (embeddings/search results are heavyweight)
- Vector operations are expensive — test with realistic corpus sizes (1M+ items)
- QMD routing must be deterministic for same input (for reproducibility)
- RAG guardrails must check facts against knowledge base before returning
- Forgetting curve parameters (decay, half-life) must be configurable per policy
- `HU_IS_TEST` guards on external API calls (embeddings, vector stores)
