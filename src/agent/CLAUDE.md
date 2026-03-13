# src/agent/ — Agent Core

The agent module implements the autonomous reasoning loop: receiving messages, planning actions, executing tools, and generating responses.

## Core Loop

```
agent.c             Main agent loop (hu_agent_t) — orchestrates turn handling
agent_turn.c        Single-turn processing (receive → think → act → respond)
agent_stream.c      Streaming response handling
dispatcher.c        Routes incoming messages to appropriate handlers
planner.c           Plans tool use sequences before execution
compaction.c        Context window compaction when tokens exceed limits
```

## Planning & Execution

```
planning.c          Conversation-level planning and strategy
dag.c               DAG-based parallel tool execution graph
dag_executor.c      Executes DAG nodes with dependency resolution
llm_compiler.c      LLMCompiler: compiles natural language plans to DAG
tool_router.c       Routes tool calls to implementations with policy checks
```

## Context & Memory Integration

```
context.c           Agent context construction
context_tokens.c    Token counting and budget management
memory_loader.c     Loads relevant memories into context
prompt.c            System prompt assembly (persona + context + memories)
max_tokens.c        Max token calculation per provider
mailbox.c           Message queuing between agents
```

## Behavioral Layer

```
superhuman.c                 Superhuman-level reasoning behaviors
superhuman_commitment.c      Commitment tracking for promises made
superhuman_emotional.c       Emotional intelligence in responses
commitment.c                 Promise/commitment extraction
commitment_store.c           Persistent commitment storage
reflection.c                 Self-reflection on past interactions
proactive.c / proactive_ext.c  Proactive outreach and suggestions
governor.c                   Rate limiting and safety governor
arbitrator.c                 Arbitrates between competing response strategies
input_guard.c                Input sanitization and safety checks
```

## Social & Awareness

```
theory_of_mind.c    Models the user's mental state and expectations
anticipatory.c      Anticipates user needs before they're expressed
awareness.c         Contextual awareness (time, location, recent events)
weather_awareness.c Weather-aware contextual responses
weather_fetch.c     Weather data retrieval
collab_planning.c   Multi-agent collaborative planning
conv_goals.c        Conversation goal tracking
team.c              Multi-agent team coordination
```

## Autonomy & Coordination

```
goals.c             Autonomous goal engine (create, decompose, prioritize, track)
orchestrator.c      Multi-agent task coordination (register agents, split tasks, assign, merge)
orchestrator_llm.c  LLM-based goal decomposition into subtasks
```

## Key Flow

```
Message arrives → dispatcher.c
  → agent_turn.c (single turn processing)
    → prompt.c (build system prompt with persona + context)
    → memory_loader.c (load relevant memories)
    → planner.c (decide tool use)
    → tool_router.c → dag.c (execute tools)
    → agent_stream.c (stream response)
    → commitment.c (extract any promises made)
    → reflection.c (post-turn reflection)
```

## Rules

- The agent loop must never block indefinitely — all operations have timeouts
- Tool execution goes through `tool_router.c` for policy checks — never call tools directly
- Context window management is critical — always check token budgets
- Use `HU_IS_TEST` for any operations that would make real API calls
