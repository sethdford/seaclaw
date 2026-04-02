# Claude Code Feature Integration Guide

This guide covers six features integrated from Claude Code architecture into h-uman: MCP Client, Hook Pipeline, Permission Tiers, Structured Compaction, Session Persistence, and Instruction File Discovery.

## Overview

These features enable h-uman agents to:
- Connect to external Model Context Protocol (MCP) servers for tool discovery and execution
- Intercept tool execution with pre/post hooks for security, auditing, and custom logic
- Enforce tiered permission levels across tool access
- Compress conversation history using structured XML summaries
- Auto-save and resume conversation sessions
- Discover and merge project-specific instruction files

All features can be individually enabled/disabled via `config.json`.

---

## 1. MCP Client (Model Context Protocol)

### What it does

The MCP client connects h-uman to external MCP servers (stdio-based processes) that expose tools. Tools discovered from MCP servers are wrapped as standard h-uman tools and follow the naming convention `mcp__<server>__<tool_name>`.

This allows seamless integration with external tool ecosystems without modifying h-uman's core codebase.

### Configuration

Add MCP servers to `config.json`:

```json
{
  "mcp_servers": [
    {
      "name": "filesystem",
      "transport_type": "stdio",
      "command": "/usr/local/bin/mcp-filesystem",
      "args": ["--root", "/home/user/workspace"],
      "auto_connect": true,
      "timeout_ms": 30000
    },
    {
      "name": "web",
      "transport_type": "stdio",
      "command": "/usr/local/bin/mcp-web",
      "auto_connect": false,
      "timeout_ms": 60000
    }
  ]
}
```

**Configuration fields:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `name` | string | Yes | Identifier for the server (used in tool names) |
| `transport_type` | string | No | `"stdio"` (default), `"sse"`, or `"http"` |
| `command` | string | For stdio | Path to the executable binary |
| `args` | array | No | Command-line arguments passed to the binary |
| `url` | string | For sse/http | Endpoint URL |
| `auto_connect` | bool | No | Automatically connect on startup (default: false) |
| `timeout_ms` | number | No | Request timeout in milliseconds (default: 30000) |

### Tool Naming Convention

Tools from MCP servers follow this pattern:

```
mcp__<server_name>__<tool_name>
```

Example: If server `filesystem` exposes a tool `read_file`, the full tool name is:
```
mcp__filesystem__read_file
```

This naming prevents collisions and makes the tool's origin clear.

### Supported Transports

- **stdio**: Binary runs as subprocess, communicates via JSON-RPC over stdin/stdout. Most common.
- **sse**: Server-Sent Events over HTTP for streaming responses.
- **http**: Standard HTTP REST endpoint.

### Lifecycle

```bash
# Auto-connected servers load tools on agent startup
./human --config config.json

# Manually connect a server
mcp_server_name="web"
mcp_server_url="/path/to/mcp-web"
```

### Tool Discovery

Tools are discovered when:
1. Agent initializes (auto-connected servers)
2. Manual connection is issued
3. Tool call is made (if server not yet connected, connection is attempted)

Discovered tools appear in the agent's available tool list with the `mcp__` prefix.

### Example: Connecting to a File Access Server

Create a local MCP server that exposes file tools:

```bash
# Start MCP server (stdio transport)
$ mcp-filesystem --root /home/user/work
```

Configure h-uman:

```json
{
  "mcp_servers": [
    {
      "name": "filesystem",
      "transport_type": "stdio",
      "command": "/usr/local/bin/mcp-filesystem",
      "args": ["--root", "/home/user/work"],
      "auto_connect": true
    }
  ]
}
```

Agent now has access to:
- `mcp__filesystem__read_file`
- `mcp__filesystem__write_file`
- `mcp__filesystem__list_directory`
- etc.

---

## 2. Hook Pipeline (Pre/Post Tool Execution)

### What it does

Hooks are shell commands executed before and/or after a tool runs. They enable:
- **Pre-tool validation**: Deny execution based on tool name/arguments
- **Security auditing**: Log all tool calls to audit trail
- **Custom policies**: Reject calls to risky tools under certain conditions
- **Post-tool logging**: Capture results for monitoring

Hooks run in registration order. The first hook that denies execution stops the pipeline.

### Exit Code Contract

Hooks communicate decisions via exit code:

| Exit Code | Decision | Behavior |
|-----------|----------|----------|
| 0 | Allow | Tool execution continues (silent) |
| 2 | Deny | Tool execution is blocked; remaining hooks skipped |
| 3 | Warn | Tool execution continues; warning is logged |
| Other | Error | Treated as Allow with warning (unless hook is `required: true`) |

If a hook is marked `required: true` and fails (non-zero exit code), the pipeline denies execution.

### Configuration

Add hooks to `config.json`:

```json
{
  "hooks": {
    "enabled": true,
    "entries": [
      {
        "name": "security_check",
        "event": "pre_tool_execute",
        "command": "/usr/local/bin/hook-security-check",
        "timeout_sec": 5,
        "required": true
      },
      {
        "name": "audit_log",
        "event": "post_tool_execute",
        "command": "logger -t human-audit",
        "timeout_sec": 10,
        "required": false
      }
    ]
  }
}
```

**Hook fields:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `name` | string | Yes | Human-readable hook identifier |
| `event` | string | Yes | `"pre_tool_execute"` or `"post_tool_execute"` |
| `command` | string | Yes | Shell command to execute |
| `timeout_sec` | number | No | Timeout in seconds (0 = default 30s) |
| `required` | bool | No | If true, execution failure → deny (default: false) |

### Hook Events

**Pre-tool hooks** run before the tool is executed. Context includes:
- Tool name
- Arguments (JSON string)
- Event type

**Post-tool hooks** run after the tool completes. Context includes:
- Tool name
- Arguments (JSON string)
- Tool output/result (string)
- Success flag (bool)

### Shell Security

Hooks execute in a shell environment with automatic escaping:

1. **Shell metacharacter escaping**: Special characters in tool names/arguments are escaped
2. **Environment variable sanitization**: Hooks run with a clean environment (no secrets passed)
3. **Timeout enforcement**: Hooks that exceed timeout are killed
4. **Subprocess isolation**: Each hook runs in its own process

### Writing Hook Scripts

Hooks receive context via environment variables:

```bash
#!/bin/bash
# Pre-tool hook example: deny shell access

TOOL_NAME="$HU_HOOK_TOOL_NAME"
TOOL_ARGS="$HU_HOOK_ARGS_JSON"

if [[ "$TOOL_NAME" == "shell" ]]; then
    # Dangerous tool
    exit 2  # DENY
fi

# Allow other tools
exit 0  # ALLOW
```

Post-tool hook with logging:

```bash
#!/bin/bash
# Post-tool hook: audit all tool results

TOOL_NAME="$HU_HOOK_TOOL_NAME"
RESULT_OUTPUT="$HU_HOOK_RESULT_OUTPUT"
RESULT_SUCCESS="$HU_HOOK_RESULT_SUCCESS"

echo "[$(date)] Tool: $TOOL_NAME, Success: $RESULT_SUCCESS" >> /var/log/human-audit.log

exit 0  # ALLOW
```

### Hook Pipeline Flow

```
Tool execution request
  ↓
Run pre-tool hooks (registration order)
  ├─ Hook 1: exit 0 (Allow) → continue
  ├─ Hook 2: exit 3 (Warn) → log warning, continue
  ├─ Hook 3: exit 2 (Deny) → STOP, reject execution
  └─ (remaining hooks skipped)
  ↓
[Execution denied]
```

---

## 3. Permission Tiers

### What it does

Permission tiers enforce graduated access control. Each tool is classified into exactly one tier; an agent can only execute tools at or below its current permission level.

This prevents agents from accidentally or maliciously accessing dangerous tools without explicit escalation.

### Permission Levels

| Level | Name | Min Value | Tools |
|-------|------|-----------|-------|
| 0 | ReadOnly | 0 | `search`, `read`, `list`, `recall`, `screenshot` |
| 1 | WorkspaceWrite | 1 | `file_write`, `shell`, `git`, `browser_navigate`, `browser_interact` |
| 2 | DangerFullAccess | 2 | `agent_spawn`, `delegate`, `cron`, `services` |

An agent with permission level 1 (WorkspaceWrite) can access:
- All level 0 tools (ReadOnly)
- All level 1 tools (WorkspaceWrite)
- **Cannot** access level 2 tools (DangerFullAccess)

### Configuration

Set agent's base permission level in `config.json`:

```json
{
  "agent": {
    "permission_level": 1
  }
}
```

| Value | Name |
|-------|------|
| 0 | ReadOnly |
| 1 | WorkspaceWrite |
| 2 | DangerFullAccess |

Default: 1 (WorkspaceWrite)

### Tool Classification

**ReadOnly (0)**:
- File/directory search and listing
- Content reading
- Memory recall
- Observation (screenshots, sensor reads)

**WorkspaceWrite (1)**:
- File creation/modification/deletion
- Shell command execution
- Git operations
- Web browser control (read pages, click elements)
- Network fetch

**DangerFullAccess (2)**:
- Agent spawning / sub-agent delegation
- Scheduled task creation (cron)
- Service management
- Permission escalation
- System integration

Unknown tools default to level 2 (deny unless explicitly allowed).

### Temporary Escalation

Escalate permission for a single tool call:

```c
// C API example
hu_permission_escalate_temporary(agent, HU_PERM_DANGER_FULL_ACCESS, "agent_spawn");
// ... tool executes ...
hu_permission_reset_escalation(agent);  // Restored to base level
```

Escalation is **temporary and automatic**: after the tool executes, the agent reverts to its base permission level.

### Deny-by-Default

Unknown tools always deny. Tool levels must be explicitly registered or tools won't execute.

---

## 4. Structured Compaction (Context Compression)

### What it does

When conversation history approaches the context window limit, h-uman automatically compacts old messages into an XML summary. The summary preserves:

- Message counts (preserved vs. summarized)
- Tool mentions (what tools were used)
- Recent user requests (last N user messages)
- Pending work inference (outstanding tasks)

Detailed messages are replaced with the structured summary, freeing context for new messages while retaining essential context.

### Configuration

Enable in `config.json`:

```json
{
  "agent": {
    "compact_context": true,
    "compaction_use_structured": true,
    "compaction_keep_recent": 20,
    "compaction_max_summary_chars": 2000,
    "compaction_max_source_chars": 5000,
    "context_pressure_warn": 0.85,
    "context_pressure_compact": 0.95,
    "context_compact_target": 0.70
  }
}
```

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `compact_context` | bool | false | Enable context compaction |
| `compaction_use_structured` | bool | false | Use XML structured summaries (vs. plain text) |
| `compaction_keep_recent` | number | 20 | Preserve N most recent messages |
| `compaction_max_summary_chars` | number | 5000 | Max size of summary XML |
| `context_pressure_warn` | float | 0.85 | Warn when context at 85% capacity |
| `context_pressure_compact` | float | 0.95 | Auto-compact when context at 95% capacity |
| `context_compact_target` | float | 0.70 | Compact until context below 70% |

### Summary Format (XML)

Structured summaries use XML:

```xml
<summary>
  <metadata>
    <total_messages>487</total_messages>
    <preserved_count>20</preserved_count>
    <summarized_count>467</summarized_count>
  </metadata>
  
  <tool_mentions>
    <tool name="file_read" count="34"/>
    <tool name="shell" count="12"/>
    <tool name="git_commit" count="8"/>
  </tool_mentions>
  
  <recent_requests>
    User asked to: refactor authentication module with OAuth2
    User asked to: add unit tests for payment gateway
    User asked to: deploy to staging environment
  </recent_requests>
  
  <pending_work>
    Outstanding: Implement JWT token refresh logic
    Outstanding: Write integration tests for API endpoints
    Outstanding: Review security audit findings
  </pending_work>
</summary>
```

This compact XML representation retains context while using ~90% less space than the original messages.

### Artifact Pinning

Files actively referenced in recent messages are "pinned" and preserved (not compacted). Examples:
- Recently edited source files
- Configuration files being modified
- Test files with assertions

If a file is pinned, all messages referencing it are kept, ensuring continuity for in-progress work.

### Continuation Preamble

When resuming a compacted session, a preamble message is injected:

> "This session is being continued from a previous conversation that was summarized due to context window limits."

Followed by the structured summary context. This reminds the agent of recent activity without losing context.

### Automatic Triggers

Compaction runs automatically when:

1. **Pressure warning** (85% capacity): Log warning, no action
2. **Auto-compact** (95% capacity): Compact old messages → reduce to 70% capacity
3. **Manual trigger**: `/compact` command

---

## 5. Session Persistence & Resume

### What it does

Sessions automatically save conversation history to JSON files. Agents can:
- **Auto-save**: After each turn (if enabled)
- **Resume**: Load a previous session to continue from the last message
- **List**: View all saved sessions with metadata
- **Delete**: Remove a session file

Sessions are stored in `~/.human/sessions/` by default.

### Configuration

Enable in `config.json`:

```json
{
  "agent": {
    "session_auto_save": true,
    "session_dir": "~/.human/sessions"
  }
}
```

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `session_auto_save` | bool | false | Auto-save after each turn |
| `session_dir` | string | `~/.human/sessions` | Directory for session JSON files |

### Session File Format

Sessions are stored as JSON with this schema:

```json
{
  "schema_version": 1,
  "metadata": {
    "id": "session_20260402_154230",
    "created_at": 1743667350,
    "model_name": "claude-3-5-sonnet",
    "workspace_dir": "/home/user/project",
    "message_count": 47
  },
  "messages": [
    {
      "role": "user",
      "content": "Help me refactor the auth module"
    },
    {
      "role": "assistant",
      "content": "I'll help you refactor the authentication module..."
    }
  ]
}
```

**Fields:**

| Field | Type | Description |
|-------|------|-------------|
| `schema_version` | number | Session format version (currently 1) |
| `metadata.id` | string | Session identifier (e.g., `session_20260402_154230`) |
| `metadata.created_at` | number | Unix timestamp (epoch seconds) |
| `metadata.model_name` | string | Model used in this session |
| `metadata.workspace_dir` | string | Workspace directory for context |
| `metadata.message_count` | number | Total messages in session |
| `messages` | array | Full conversation history |

### Storage Location

By default, sessions are stored in:

```
~/.human/sessions/
  ├── session_20260402_154230.json
  ├── session_20260402_155045.json
  └── session_20260402_160012.json
```

Each session is a single JSON file named `<session_id>.json`.

### Commands

**Save current session:**
```bash
/save-session
```

Output: `Session saved: session_20260402_154230`

The session ID is generated automatically as `session_YYYYMMDD_HHMMSS`.

**Resume a previous session:**
```bash
/resume session_20260402_154230
```

This loads the entire message history from the session file, picking up where the conversation left off.

**List all sessions:**
```bash
/list-sessions
```

Output:
```
Saved Sessions:
1. session_20260402_154230 (created 2026-04-02 15:42, 47 messages, model: claude-3-5-sonnet)
2. session_20260402_155045 (created 2026-04-02 15:50, 23 messages, model: claude-3-5-sonnet)
3. session_20260402_160012 (created 2026-04-02 16:00, 12 messages, model: gpt-4o)
```

**Delete a session:**
```bash
/delete-session session_20260402_154230
```

### Auto-Save Behavior

When `session_auto_save: true`:

1. After each agent turn completes, session is saved
2. Session ID is generated if not already set
3. File is written atomically (temp file → rename)
4. Agent continues without blocking on save

This ensures conversation is never lost due to crashes or disconnects.

### Atomic Writes

Session saves are **atomic**:

1. Write to temporary file (`<session_id>.json.tmp`)
2. Rename atomically to final location (`<session_id>.json`)

If the process crashes mid-write, the previous version is untouched.

### File Size Limits

Max session file size: 1 GB (prevents unbounded growth)

---

## 6. Instruction File Discovery

### What it does

Instruction files are Markdown documents that provide project-specific context and guidelines to agents. h-uman automatically discovers and merges instructions from multiple sources in a priority order.

This allows teams to embed instructions at the project level, workspace level, and user level—h-uman intelligently merges them.

### Discovery Order (High → Low Priority)

1. **Workspace-level**: `<workspace_dir>/.human.md` (highest priority)
2. **Project root**: `HUMAN.md` in ancestor directories (walk upward max 10 levels)
3. **User home**: `~/.human/instructions.md` (lowest priority)

If multiple files exist, their content is merged with later discoveries taking precedence for duplicate rules.

### Configuration

Enable in `config.json`:

```json
{
  "agent": {
    "discover_instructions": true
  }
}
```

### File Limits

- **Per-file limit**: 4,000 characters max per instruction file
- **Total limit**: 12,000 characters max across all merged files
- **Walk depth**: Maximum 10 ancestor directories searched for `HUMAN.md`

Files exceeding the per-file limit are truncated; the agent is notified.

### Example: Creating Instructions

**Workspace-level (.human.md):**

```markdown
# Authentication Refactor — Phase 2

## Do NOT
- Remove legacy OAuth endpoints (still used by mobile app)
- Change JWT expiration without updating refresh token logic

## DO
- Use RFC 7231 HTTP conventions
- Add unit tests for all new auth flows
- Document breaking changes in CHANGELOG.md

## Context
We're gradually migrating from custom auth to OpenID Connect.
Legacy system must remain functional during transition (6 months).

## Files to avoid modifying
- src/legacy/oauth1.c — deprecated, will remove in v3.0
```

**Project root (HUMAN.md):**

```markdown
# h-uman Engineering Standards

## Naming
- Functions: snake_case
- Types: hu_<name>_t
- Constants: HU_SCREAMING_SNAKE

## Testing
- All features require unit tests
- No real network access in tests (use mocks)
- Target: 90%+ code coverage

## Security
- Deny-by-default for permissions
- No secrets in logs or config files
- HTTPS-only for external connections
```

**User home (~/.human/instructions.md):**

```markdown
# Personal Claude Code Preferences

## Avoid
- Creating new files unless necessary
- Commenting obvious code
- Long explanations (prefer examples)

## Prefer
- Editing existing files
- Showing working code
- Short summaries
```

### Discovery and Merge

On agent startup:

1. Check `<workspace_dir>/.human.md` → read (max 4000 chars)
2. Walk ancestors for `HUMAN.md` → read each match (max 4000 chars each)
3. Check `~/.human/instructions.md` → read (max 4000 chars)
4. Merge all files (workspace highest priority) → single string (max 12000 chars)
5. Inject into system prompt

Agent now has full context from all three levels without manual copy-paste.

### Security (Path Validation)

Path safety checks prevent directory traversal:

1. **Null byte detection**: Reject paths containing `\0`
2. **Canonicalization**: Use `realpath()` to resolve symlinks and `..` components
3. **Symlink detection**: Reject symlinks that point outside workspace

This prevents malicious instructions from escaping the project directory.

### Cache and Freshness

Instructions are cached for 5 minutes (TTL: 300 seconds). The cache is invalidated if:

- Any instruction file's modification time changes
- Any file is deleted
- TTL expires

This balances performance (avoid reading disk every turn) with freshness (pick up changes quickly during development).

### Example: Updating Instructions

Edit workspace instructions during a session:

```bash
$ echo "## New guideline: Always use async/await" >> .human.md
```

Within 5 minutes (or immediately on next manual refresh), the agent picks up the new guideline.

---

## Configuration Reference

### Full config.json Example (All 6 Features)

```json
{
  "workspace": "~/.human/workspace",
  "default_provider": "anthropic",
  "default_model": "claude-3-5-sonnet-20241022",

  "mcp_servers": [
    {
      "name": "filesystem",
      "transport_type": "stdio",
      "command": "/usr/local/bin/mcp-filesystem",
      "args": ["--root", "~/.human/workspace"],
      "auto_connect": true,
      "timeout_ms": 30000
    },
    {
      "name": "web",
      "transport_type": "stdio",
      "command": "/usr/local/bin/mcp-web",
      "auto_connect": false
    }
  ],

  "hooks": {
    "enabled": true,
    "entries": [
      {
        "name": "security_check",
        "event": "pre_tool_execute",
        "command": "/usr/local/bin/hooks/security-check.sh",
        "timeout_sec": 5,
        "required": true
      },
      {
        "name": "audit_log",
        "event": "post_tool_execute",
        "command": "logger -t human-tools",
        "timeout_sec": 10,
        "required": false
      }
    ]
  },

  "agent": {
    "permission_level": 1,
    "session_auto_save": true,
    "session_dir": "~/.human/sessions",
    "discover_instructions": true,
    "compact_context": true,
    "compaction_use_structured": true,
    "compaction_keep_recent": 20,
    "compaction_max_summary_chars": 2000,
    "context_pressure_warn": 0.85,
    "context_pressure_compact": 0.95,
    "context_compact_target": 0.70
  },

  "providers": [
    {
      "name": "anthropic",
      "api_key": "${ANTHROPIC_API_KEY}"
    }
  ]
}
```

### Defaults

If not specified in config.json:

| Field | Default Value |
|-------|---------------|
| `mcp_servers` | Empty (no servers) |
| `hooks.enabled` | false |
| `agent.permission_level` | 1 (WorkspaceWrite) |
| `agent.session_auto_save` | false |
| `agent.session_dir` | `~/.human/sessions` |
| `agent.discover_instructions` | false |
| `agent.compact_context` | false |
| `agent.compaction_use_structured` | false |
| `agent.compaction_keep_recent` | 20 |
| `agent.context_pressure_warn` | 0.85 |
| `agent.context_pressure_compact` | 0.95 |
| `agent.context_compact_target` | 0.70 |

---

## Security Considerations

### Shell Escaping in Hooks

Hook commands are executed in a shell. Arguments containing special characters are automatically escaped:

```bash
# Original tool call
tool: "shell", args: {"cmd": "echo 'Hello & goodbye'"}

# Hook receives properly escaped version
$HU_HOOK_TOOL_NAME="shell"
$HU_HOOK_ARGS_JSON='{"cmd":"echo '"'"'Hello & goodbye'"'"'"}'

# Safe to use in shell without further escaping
```

### Path Traversal Protection

Instruction file paths are validated:

1. Canonicalize with `realpath()` (follows symlinks, resolves `..`)
2. Reject if result is outside workspace
3. Reject if path contains `\0` bytes
4. Reject symlinks pointing outside workspace

This prevents malicious `HUMAN.md` files in subdirectories from escaping the project.

### Permission Tier Deny-by-Default

Unknown tools cannot execute, even if accidentally misconfigured:

```
Tool classification missing?
→ Default to DangerFullAccess (level 2)
→ Only agents with level 2 can execute
→ Most agents have level 1 → Deny
```

This fail-safe prevents unexpected access.

### MCP Server Sandboxing

MCP server processes are:

- Spawned as separate processes (not in-memory)
- Killed if timeout exceeded
- Terminated when h-uman shuts down
- Run with inherited environment (can filter secrets)

### Atomic Session Writes

Session saves cannot create partial/corrupt files:

1. Write to temp file
2. Rename atomically (single syscall)
3. Previous file untouched if crash occurs

### Instruction File TTL Caching

Instructions are cached for 5 minutes, preventing excessive disk I/O:

- **Upside**: Performance (no disk read every turn)
- **Downside**: Changes take up to 5 minutes to propagate

For instant updates, restart the agent or manually trigger a refresh.

---

## 7. Configuration Hot-Reload

### What it does

Configuration hot-reload allows you to update h-uman's settings without restarting the process. External tools and automation systems can trigger a reload by sending a SIGHUP signal to the running agent, or users can manually reload configuration using the `/reload-config` command in the CLI.

This is useful for:
- Updating hooks, permissions, or MCP server configurations dynamically
- Refreshing instruction files without restarting
- Deploying configuration changes to production agents
- Automation systems that need to reconfigure agents on-the-fly

### Signal-Based Reload (POSIX Systems)

Send SIGHUP to the h-uman process to trigger a config reload:

```bash
# Find the h-uman process
PID=$(pgrep human)

# Send SIGHUP to trigger reload
kill -HUP $PID
```

The agent will:
1. Re-parse `~/.human/config.json` or the specified config file
2. Rebuild hook registry from updated hook definitions
3. Re-evaluate and update permission levels
4. Re-discover instruction files from workspace
5. Print a summary of what changed to stderr

Example output:

```
[human] Config reloaded via SIGHUP
Config sections updated: hooks (2 added), permissions (updated), instructions (3 files)
```

### Manual CLI Reload Command

Users can also reload configuration interactively using:

```
> /reload-config
Config reloaded. Updated: hooks, permissions, instructions.
```

### What Gets Reloaded

When a reload is triggered, the following are updated:

| Component | Behavior |
|-----------|----------|
| **Hooks** | Rebuilt from config; existing hooks cleared and re-registered |
| **Permissions** | Base permission level re-evaluated from config |
| **Instructions** | Workspace `.human.md` and parent `HUMAN.md` files re-discovered |
| **MCP Servers** | Loaded from config (auto-connect servers re-initialized) |

Static agent properties (model, provider, tools) are NOT reloaded; restart is required for those.

### What Does NOT Get Reloaded

The following require a full agent restart:

- Default provider and API keys
- Default model name
- Tool definitions (use MCP server reload for new tools)
- Allocator or memory backend configuration

### Limitations

- SIGHUP is only available on POSIX systems (Linux, macOS, BSD). Windows users must restart to reload configuration.
- Configuration hot-reload happens between conversation turns. If a turn is running when SIGHUP is received, the reload processes after that turn completes.
- Errors during reload are logged to stderr but don't interrupt agent operation.

### Example: Update Hooks Without Restart

Edit `config.json` to add a new hook:

```json
{
  "hooks": {
    "entries": [
      {
        "name": "new_security_check",
        "event": "pre_tool_execute",
        "command": "/usr/local/bin/new-hook.sh",
        "required": true,
        "timeout_sec": 5
      }
    ]
  }
}
```

Then trigger reload:

```bash
kill -HUP $(pgrep human)
```

The new hook is now active without interrupting the conversation.

---

## Troubleshooting

### MCP Server Not Connecting

**Problem**: `mcp__server__tool` tools don't appear in available tools list.

**Solutions**:

1. Verify server is running:
   ```bash
   /usr/local/bin/mcp-filesystem --root /tmp
   # Should accept stdin without hanging
   ```

2. Check config transport_type matches server:
   - stdio server → `"transport_type": "stdio"`
   - HTTP server → `"transport_type": "http"` with URL

3. If `auto_connect: false`, manually connect:
   ```bash
   /mcp-connect filesystem
   ```

4. Check agent logs for connection errors (stderr output)

### Hooks Not Executing

**Problem**: Pre/post-tool hooks don't run.

**Solutions**:

1. Verify `hooks.enabled: true` in config
2. Check hook command is executable:
   ```bash
   chmod +x /usr/local/bin/hooks/my-hook.sh
   ```

3. Verify event type (`pre_tool_execute` vs `post_tool_execute`)

4. Check hook timeouts (if too short, hook is killed before completing)

### Permission Denied on Tool

**Problem**: Tool execution fails with "permission denied".

**Solutions**:

1. Check agent's permission level:
   ```json
   "agent": { "permission_level": 1 }
   ```

2. Check tool's required level (internal table or logs)

3. Request escalation via `/escalate <tool_name>` (if supported by agent)

### Session Not Saving

**Problem**: Sessions don't persist across restarts.

**Solutions**:

1. Verify `session_auto_save: true` and `session_dir` is writable
2. Check directory exists and is readable/writable:
   ```bash
   mkdir -p ~/.human/sessions
   chmod 700 ~/.human/sessions
   ```

3. Manually save with `/save-session`

4. Check disk space (1 GB limit per session)

### Instructions Not Applied

**Problem**: `.human.md` changes not reflected in agent behavior.

**Solutions**:

1. Verify `discover_instructions: true` in config

2. Restart agent or wait 5 minutes (cache TTL)

3. Verify file exists and is readable:
   ```bash
   ls -la /path/to/workspace/.human.md
   ```

4. Check file size (max 4000 chars per file, 12000 total)

---

## Further Reading

For implementation details and advanced configuration, see:

- `include/human/mcp_manager.h` — MCP client API
- `include/human/hook.h` + `hook_pipeline.h` — Hook system
- `include/human/permission.h` — Permission tier implementation
- `include/human/agent/compaction_structured.h` — Structured summary format
- `include/human/agent/session_persist.h` — Session persistence API
- `include/human/agent/instruction_discover.h` — Instruction discovery
- `docs/standards/ai/` — AI architecture and agent standards
- `docs/standards/security/` — Security model and threat analysis
