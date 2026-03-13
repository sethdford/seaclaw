# src/tools/ — Tool Implementations

67+ tools that the agent can execute. Each tool implements the `hu_tool_t` vtable.

## Vtable Contract

Every tool must implement:

- `execute(ctx, params_json)` → `hu_tool_result_t`
- `name()` → stable lowercase string (e.g., `"shell"`)
- `description()` → human-readable description
- `parameters_json()` → JSON Schema for parameters

## Registration

All tools register in `factory.c`. Add your tool's header include and factory entry there.

## Tool Categories

### File System

```
file_read.c       Read file contents
file_write.c      Write/create files
file_edit.c       Edit file sections
file_append.c     Append to files
diff.c            Show file diffs
```

### Shell & Process

```
shell.c           Execute shell commands (sandboxed)
```

### Web & Network

```
http_request.c    Make HTTP requests
web_fetch.c       Fetch and parse web pages
tavily.c          Tavily web search
```

### Memory

```
memory_recall.c   Recall from memory
memory_store.c    Store to memory
save_for_later.c  Save content for later retrieval
```

### Communication

```
send_message.c    Send message to user/channel
broadcast.c       Send to multiple channels
delegate.c        Delegate to another agent
```

### Agent Coordination

```
agent_spawn.c     Spawn sub-agents
agent_query.c     Query other agents
```

### Calendar & Scheduling

```
calendar.c        Calendar operations
cron_add.c        Add cron jobs
cron_remove.c     Remove cron jobs
cron_list.c       List cron jobs
cron_run.c        Run cron job manually
cron_update.c     Update cron jobs
cron_runs.c       View cron run history
```

### Business & External

```
crm.c             CRM operations
firebase.c        Firebase operations
gcloud.c          Google Cloud operations
facebook.c        Facebook API
analytics.c       Analytics operations
composio.c        Composio integration
```

### Advanced

```
browser.c         Browser automation (HU_ENABLE_TOOLS_BROWSER)
browser_open.c    Open URL in browser
screenshot.c      Take screenshots
canvas.c          Canvas rendering
notebook.c        Notebook operations
database.c        Database queries
claude_code.c     Claude Code integration
apply_patch.c     Apply code patches
```

## Security Rules (High Risk)

- Validate ALL inputs before use — paths, URLs, commands, parameters
- Add `HU_IS_TEST` guard for tools that spawn processes or open network connections
- Never execute user-supplied strings as shell commands without sanitization
- URL tools: reject non-HTTPS schemes
- File tools: validate paths are within allowed directories
- Shell tool: runs through sandbox policy — never bypass

## Adding a New Tool

1. Create `src/tools/<name>.c` implementing `hu_tool_t`
2. Create `include/human/tools/<name>.h`
3. Register in `src/tools/factory.c`
4. Add tests in `tests/test_tools_all.c` or a dedicated test file
5. Add to CMakeLists.txt source list (with feature guard if optional)
