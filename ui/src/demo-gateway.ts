import type { GatewayStatus, ServerFeatures } from "./gateway.js";

const DEMO_CHANNELS = [
  { key: "telegram", label: "Telegram", configured: true, healthy: true, status: "Connected" },
  { key: "discord", label: "Discord", configured: true, healthy: true, status: "Connected" },
  { key: "slack", label: "Slack", configured: true, healthy: true, status: "Connected" },
  { key: "signal", label: "Signal", configured: false, status: "Not configured" },
  { key: "imessage", label: "iMessage", configured: true, healthy: true, status: "Connected" },
  {
    key: "email",
    label: "Email",
    configured: true,
    healthy: false,
    error: "SMTP timeout",
    status: "Error",
  },
  { key: "matrix", label: "Matrix", configured: false, status: "Not configured" },
  { key: "whatsapp", label: "WhatsApp", configured: true, healthy: true, status: "Connected" },
];

const DEMO_SESSIONS = [
  {
    key: "sess-a1",
    label: "Project Planning",
    created_at: Date.now() - 3600000,
    last_active: Date.now() - 120000,
    turn_count: 24,
  },
  {
    key: "sess-b2",
    label: "Code Review",
    created_at: Date.now() - 7200000,
    last_active: Date.now() - 600000,
    turn_count: 12,
  },
  {
    key: "sess-c3",
    label: "Bug Investigation",
    created_at: Date.now() - 86400000,
    last_active: Date.now() - 3600000,
    turn_count: 8,
  },
  {
    key: "sess-d4",
    label: "Architecture Discussion",
    created_at: Date.now() - 172800000,
    last_active: Date.now() - 86400000,
    turn_count: 31,
  },
  {
    key: "sess-e5",
    label: "Quick Question",
    created_at: Date.now() - 259200000,
    last_active: Date.now() - 172800000,
    turn_count: 3,
  },
];

const DEMO_EVENTS = [
  {
    type: "message",
    channel: "Telegram",
    user: "Alice",
    preview: "Can you review the PR?",
    time: Date.now() - 30000,
  },
  { type: "tool_exec", tool: "shell", command: "git status", time: Date.now() - 60000 },
  {
    type: "message",
    channel: "Discord",
    user: "Bob",
    preview: "Deploy looks good",
    time: Date.now() - 120000,
  },
  { type: "session_start", session: "Code Review", time: Date.now() - 180000 },
  {
    type: "message",
    channel: "Slack",
    user: "Charlie",
    preview: "Tests passing on CI",
    time: Date.now() - 240000,
  },
  {
    type: "tool_exec",
    tool: "web_search",
    command: "latest Rust async patterns",
    time: Date.now() - 300000,
  },
];

function handleRequest(method: string, _params?: Record<string, unknown>): unknown {
  switch (method) {
    case "connect":
      return {
        type: "hello-ok",
        server: { version: "0.42.0" },
        features: { methods: [], sessions: true, cron: true, skills: true, cost_tracking: true },
      };
    case "health":
      return { status: "operational" };
    case "capabilities":
      return { version: "0.42.0", tools: 53, channels: 20, providers: 50 };
    case "channels.status":
      return { channels: DEMO_CHANNELS };
    case "sessions.list":
      return { sessions: DEMO_SESSIONS };
    case "update.check":
      return { available: false, current_version: "0.42.0" };
    case "chat.history":
      return {
        messages: [
          { role: "user", content: "Can you help me debug this memory leak?" },
          {
            role: "assistant",
            content:
              "Of course! Let me analyze the code. I'll start by checking the allocation patterns in the hot path.\n\nI found the issue — there's a missing `free()` call in the error branch of `sc_json_parse`. The allocated buffer leaks when parsing fails halfway through.",
          },
          { role: "user", content: "Great catch! Can you fix it?" },
          {
            role: "assistant",
            content:
              "Done. I've added the cleanup in the error path and added a regression test. All 2211 tests pass with 0 ASan errors.",
          },
        ],
      };
    case "activity.recent":
      return { events: DEMO_EVENTS };
    case "config.get":
      return {
        exists: true,
        raw: JSON.stringify(
          {
            default_provider: "openai",
            default_model: "gpt-4o",
            workspace_dir: "~/projects",
          },
          null,
          2,
        ),
        workspace_dir: "~/projects",
        default_provider: "openai",
        default_model: "gpt-4o",
        max_tokens: 0,
        temperature: 0.7,
      };
    case "config.schema":
      return { schema: { type: "object", properties: {} } };
    case "config.set":
      return { saved: true };
    case "tools.catalog":
      return {
        tools: [
          { name: "shell", description: "Execute shell commands", category: "system" },
          { name: "file_read", description: "Read file contents", category: "filesystem" },
          { name: "file_write", description: "Write file contents", category: "filesystem" },
          { name: "git", description: "Git operations", category: "dev" },
          { name: "web_search", description: "Search the web", category: "web" },
          { name: "web_fetch", description: "Fetch URL contents", category: "web" },
        ],
      };
    case "models.list":
      return {
        models: [
          { id: "gpt-4o", provider: "openai", context_window: 128000 },
          { id: "claude-sonnet-4-20250514", provider: "anthropic", context_window: 200000 },
          { id: "gemini-2.5-pro", provider: "gemini", context_window: 1000000 },
        ],
        providers: [
          {
            name: "openai",
            has_key: true,
            base_url: "https://api.openai.com",
            native_tools: true,
            is_default: true,
          },
          {
            name: "anthropic",
            has_key: true,
            base_url: "https://api.anthropic.com",
            native_tools: true,
            is_default: false,
          },
          {
            name: "gemini",
            has_key: true,
            base_url: "https://api.gemini.google.com",
            native_tools: true,
            is_default: false,
          },
        ],
        default_model: "gpt-4o",
      };
    case "nodes.list":
      return {
        nodes: [
          {
            id: "local",
            hostname: "localhost",
            status: "healthy",
            uptime_seconds: 86400,
            type: "gateway",
          },
        ],
      };
    case "skills.list":
      return {
        skills: [
          { name: "code-review", enabled: true, description: "Review code for issues" },
          { name: "test-gen", enabled: false, description: "Generate unit tests" },
        ],
      };
    case "skills.enable":
    case "skills.disable":
    case "skills.install":
      return { ok: true };
    case "usage.summary":
      return {
        session_cost_usd: 0.42,
        daily_cost_usd: 3.14,
        monthly_cost_usd: 28.5,
        total_tokens: 125000,
        request_count: 47,
      };
    case "cron.list":
      return {
        jobs: [
          {
            id: 1,
            name: "Daily Backup",
            schedule: "0 2 * * *",
            enabled: true,
            last_run: Date.now() - 86400000,
          },
        ],
      };
    case "cron.runs":
      return { runs: [] };
    case "cron.add":
      return { id: Date.now() };
    case "cron.update":
    case "cron.run":
    case "cron.remove":
      return { ok: true };
    case "sessions.patch":
    case "sessions.delete":
      return { ok: true };
    case "chat.send":
      return { status: "ok", sessionKey: "demo-session" };
    case "persona.set":
      return { ok: true };
    default:
      return {};
  }
}

export class DemoGatewayClient extends EventTarget {
  #status: GatewayStatus = "disconnected";
  #features: ServerFeatures = {};
  #interval: ReturnType<typeof setInterval> | null = null;

  static readonly EVENT_STATUS = "status";
  static readonly EVENT_MESSAGE = "message";
  static readonly EVENT_GATEWAY = "gateway";

  get status(): GatewayStatus {
    return this.#status;
  }
  get features(): ServerFeatures {
    return this.#features;
  }

  connect(_url: string): void {
    this.#setStatus("connecting");
    setTimeout(() => {
      this.#setStatus("connected");
      this.#features = {
        methods: [],
        sessions: true,
        cron: true,
        skills: true,
        cost_tracking: true,
      };
      this.dispatchEvent(new CustomEvent("features", { detail: this.#features }));
      this.#startActivityStream();
    }, 400);
  }

  #setStatus(s: GatewayStatus): void {
    if (this.#status === s) return;
    this.#status = s;
    this.dispatchEvent(new CustomEvent(DemoGatewayClient.EVENT_STATUS, { detail: s }));
  }

  #startActivityStream(): void {
    const channels = ["Telegram", "Discord", "Slack", "iMessage", "WhatsApp"];
    const users = ["Alice", "Bob", "Charlie", "Dana", "Eve"];
    const previews = [
      "Deployment completed successfully",
      "Can you check the logs?",
      "PR #42 merged",
      "Tests are green!",
      "New feature request from client",
      "Memory usage looks stable",
    ];
    let idx = 0;
    this.#interval = setInterval(() => {
      const event = {
        type: "message",
        channel: channels[idx % channels.length],
        user: users[idx % users.length],
        preview: previews[idx % previews.length],
        time: Date.now(),
      };
      this.dispatchEvent(
        new CustomEvent(DemoGatewayClient.EVENT_GATEWAY, {
          detail: { event: "activity", payload: event },
        }),
      );
      idx++;
    }, 5000);
  }

  async request<T = unknown>(method: string, params?: Record<string, unknown>): Promise<T> {
    await new Promise((r) => setTimeout(r, 80 + Math.random() * 200));
    return handleRequest(method, params) as T;
  }

  abort(): Promise<{ aborted?: boolean }> {
    return Promise.resolve({ aborted: true });
  }

  disconnect(): void {
    if (this.#interval) clearInterval(this.#interval);
    this.#setStatus("disconnected");
  }
}
