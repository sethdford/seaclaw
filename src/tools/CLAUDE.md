# src/tools/ — Tool Implementations (105 tools)

Executable actions available to the agent. Each implements the `hu_tool_t` vtable.

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
file_edit.c       Edit file sections (targeted replacements)
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
web_search.c      Unified web search dispatcher
web_search_providers/
  brave.c         Brave Search
  duckduckgo.c    DuckDuckGo Search
  exa.c           Exa Search
  firecrawl.c     Firecrawl Search
  jina.c          Jina Search
  perplexity.c    Perplexity Search
  searxng.c       SearXNG Search
  tavily.c        Tavily Search
  common.c        Shared web search utilities
```

### Memory & Knowledge

```
memory_recall.c   Recall from memory
memory_store.c    Store to memory
memory_list.c     List memories
memory_forget.c   Remove memories
save_for_later.c  Save content for later retrieval
bff_memory.c      Backend-for-frontend memory operations
cache_ttl.c       Tool result caching with TTL
```

### Communication & Messaging

```
send_message.c         Send message to user/channel
broadcast.c            Send to multiple channels
send_voice_message.c   Send voice messages
ask_user.c             Ask user for input (interactive)
delegate.c             Delegate to another agent
message.c              Message formatting and parsing
```

### Agent & Teams

```
agent_spawn.c     Spawn sub-agents
agent_query.c     Query other agents
cron_session_tools.c  Session-scoped cron tools
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

### Business & External APIs

```
crm.c             CRM operations
firebase.c        Firebase operations
gcloud.c          Google Cloud operations
facebook.c        Facebook API
instagram.c       Instagram API
twitter.c         Twitter/X API
analytics.c       Analytics operations
composio.c        Composio integration
jira.c            Jira issue operations
invoice.c         Invoice generation/parsing
social.c          Social media operations
spreadsheet.c     CSV/TSP spreadsheet operations
report.c          Structured report generation
workflow.c        DAG-based workflow execution
homeassistant.c   Home Assistant integration
webhook_tools.c   Webhook management and triggering
```

### Code & Development

```
git.c             Git operations
lsp.c             Language Server Protocol
code_sandbox.c    Sandboxed code execution
claude_code.c     Claude Code integration
apply_patch.c     Apply code patches
db_introspect.c   Database schema introspection
schema.c          JSON Schema operations
schema_clean.c    Schema cleanup utilities
```

### Skills, Scheduling & Agents

```
skill_run.c       Run installed skills
skill_write.c     Create/write skills
persona.c         Persona management
schedule.c        Scheduling operations
cron_add.c        Add cron jobs
cron_remove.c     Remove cron jobs
cron_list.c       List cron jobs
cron_run.c        Run cron job manually
cron_update.c     Update cron jobs
cron_runs.c       View cron run history
task_tools.c      Task list operations
```

### Browser & Computer Use

```
browser.c         Browser automation (HU_ENABLE_TOOLS_BROWSER)
browser_use.c     Advanced browser operations (page interaction, form filling)
browser_open.c    Open URL in browser
computer_use.c    Computer use automation (mouse, keyboard)
gui_agent.c       GUI agent operations
screenshot.c      Take screenshots
canvas.c          Canvas rendering (A2UI)
notebook.c        Notebook operations
pwa.c             PWA bridge operations
visual_grounding.c  Grounding visual elements in UI
```

### Media & Vision

```
image.c           Image operations
image_gen.c       Image generation (DALL-E, Gemini)
media_image.c     Image processing and manipulation
media_video.c     Video processing
media_gif.c       GIF creation and manipulation
vision_ocr.c      Optical character recognition (text extraction)
pdf.c             PDF extraction and manipulation
```

### Data & Hardware

```
database.c        Database queries
hardware_info.c   Hardware information
hardware_memory.c Hardware memory operations
i2c.c             I2C bus operations (Linux)
spi.c             SPI bus operations (Linux)
peripheral_ctrl.c Peripheral control
```

### Utility & Infrastructure

```
validation.c      Input validation
path_security.c   Path security checks
spawn.c           Process spawning
cli_wrapper_common.c  CLI wrapper shared code
tool_search.c     Tool search and discovery
mcp_resource_tools.c  MCP resource tool bridge
pushover.c        Pushover notifications
paperclip.c       Paperclip integration (attachment hosting)
doc_ingest.c      Document ingestion pipeline
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
