import { LitElement, html, css } from "lit";
import { customElement, state } from "lit/decorators.js";
import type { GatewayClient, GatewayStatus } from "./gateway.js";
import { GatewayClient as GatewayClientClass } from "./gateway.js";
import { setGateway } from "./gateway-provider.js";
import "./components/floating-mic.js";
import "./views/overview-view.js";
import "./views/chat-view.js";
import "./views/agents-view.js";
import "./views/sessions-view.js";
import "./views/models-view.js";
import "./views/config-view.js";
import "./views/tools-view.js";
import "./views/channels-view.js";
import "./views/cron-view.js";
import "./views/skills-view.js";
import "./views/voice-view.js";
import "./views/nodes-view.js";
import "./views/usage-view.js";
import "./views/logs-view.js";

type TabId =
  | "overview"
  | "chat"
  | "agents"
  | "sessions"
  | "models"
  | "config"
  | "tools"
  | "channels"
  | "cron"
  | "skills"
  | "voice"
  | "nodes"
  | "usage"
  | "logs";

const TABS: { id: TabId; label: string }[] = [
  { id: "overview", label: "Overview" },
  { id: "chat", label: "Chat" },
  { id: "agents", label: "Agents" },
  { id: "sessions", label: "Sessions" },
  { id: "models", label: "Models" },
  { id: "voice", label: "Voice" },
  { id: "nodes", label: "Nodes" },
  { id: "config", label: "Config" },
  { id: "tools", label: "Tools" },
  { id: "channels", label: "Channels" },
  { id: "cron", label: "Cron" },
  { id: "skills", label: "Skills" },
  { id: "usage", label: "Usage" },
  { id: "logs", label: "Logs" },
];

@customElement("sc-app")
export class ScApp extends LitElement {
  static override styles = css`
    :host {
      display: block;
      height: 100vh;
      font-family: var(--sc-font);
    }
    .layout {
      display: flex;
      flex-direction: column;
      height: 100%;
    }
    .nav {
      display: flex;
      align-items: center;
      gap: 0.5rem;
      padding: 0.5rem 1rem;
      background: var(--sc-bg-surface);
      border-bottom: 1px solid var(--sc-border);
    }
    .nav-tabs {
      display: flex;
      gap: 0.25rem;
      flex-wrap: wrap;
    }
    .nav-tab {
      padding: 0.5rem 0.75rem;
      background: transparent;
      border: none;
      color: var(--sc-text-muted);
      cursor: pointer;
      border-radius: var(--sc-radius);
      font-size: 0.875rem;
    }
    .nav-tab:hover {
      color: var(--sc-text);
      background: var(--sc-bg-elevated);
    }
    .nav-tab.active {
      color: var(--sc-accent);
      background: var(--sc-bg-elevated);
    }
    .status {
      margin-left: auto;
      display: flex;
      align-items: center;
      gap: 0.5rem;
      font-size: 0.75rem;
      color: var(--sc-text-muted);
    }
    .status-dot {
      width: 8px;
      height: 8px;
      border-radius: 50%;
    }
    .status-dot.connected {
      background: var(--sc-success);
    }
    .status-dot.connecting {
      background: var(--sc-warning);
      animation: pulse 1s ease-in-out infinite;
    }
    .status-dot.disconnected {
      background: var(--sc-text-muted);
    }
    @keyframes pulse {
      0%,
      100% {
        opacity: 1;
      }
      50% {
        opacity: 0.4;
      }
    }
    .main {
      flex: 1;
      overflow: auto;
      padding: 1rem;
      background: var(--sc-bg);
    }
  `;

  @state() private tab: TabId = "overview";
  @state() private chatSessionKey = "default";
  @state() private connectionStatus: GatewayStatus = "disconnected";

  gateway: GatewayClient | null = null;

  override firstUpdated(): void {
    this.gateway = new GatewayClientClass();
    setGateway(this.gateway);
    this.gateway.addEventListener("status", ((
      e: CustomEvent<GatewayStatus>,
    ) => {
      this.connectionStatus = e.detail;
    }) as EventListener);
    const wsUrl =
      typeof window !== "undefined" &&
      (window as unknown as { __VITE_WS_PROXY__?: string }).__VITE_WS_PROXY__
        ? (window as unknown as { __VITE_WS_PROXY__: string }).__VITE_WS_PROXY__
        : (() => {
            const proto =
              window.location.protocol === "https:" ? "wss:" : "ws:";
            const host = window.location.host;
            return `${proto}//${host}/ws`;
          })();
    this.gateway.connect(wsUrl);

    this.addEventListener("navigate", ((e: CustomEvent<string>) => {
      const raw = e.detail as string;
      const [tabPart, sessionPart] = raw.includes(":")
        ? (raw.split(":") as [string, string])
        : [raw, undefined];
      const target = tabPart as TabId;
      if (TABS.some((t) => t.id === target)) {
        this.tab = target;
        if (target === "chat") {
          this.chatSessionKey = sessionPart ?? "default";
        }
      }
    }) as EventListener);
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.gateway?.disconnect();
  }

  private handleTabKeydown(e: KeyboardEvent): void {
    const tabs = TABS.map((t) => t.id);
    const currentIdx = tabs.indexOf(this.tab);
    let newIdx = currentIdx;
    if (e.key === "ArrowRight" || e.key === "ArrowDown") {
      e.preventDefault();
      newIdx = (currentIdx + 1) % tabs.length;
    } else if (e.key === "ArrowLeft" || e.key === "ArrowUp") {
      e.preventDefault();
      newIdx = (currentIdx - 1 + tabs.length) % tabs.length;
    } else if (e.key === "Home") {
      e.preventDefault();
      newIdx = 0;
    } else if (e.key === "End") {
      e.preventDefault();
      newIdx = tabs.length - 1;
    } else {
      return;
    }
    this.tab = tabs[newIdx];
    this.updateComplete.then(() => {
      const btn = this.shadowRoot?.querySelector(
        `.nav-tab[aria-selected="true"]`,
      ) as HTMLElement | null;
      btn?.focus();
    });
  }

  override render() {
    return html`
      <div class="layout">
        <nav class="nav">
          <div
            class="nav-tabs"
            role="tablist"
            aria-label="Navigation"
            @keydown=${this.handleTabKeydown}
          >
            ${TABS.map(
              (t) => html`
                <button
                  class="nav-tab ${this.tab === t.id ? "active" : ""}"
                  role="tab"
                  aria-selected="${this.tab === t.id}"
                  tabindex="${this.tab === t.id ? 0 : -1}"
                  @click=${() => (this.tab = t.id)}
                >
                  ${t.label}
                </button>
              `,
            )}
          </div>
          <div class="status">
            <span class="status-dot ${this.connectionStatus}"></span>
            ${this.connectionStatus}
          </div>
        </nav>
        <main class="main">
          ${this.tab === "overview"
            ? html`<sc-overview-view></sc-overview-view>`
            : ""}
          ${this.tab === "chat"
            ? html`<sc-chat-view
                .sessionKey=${this.chatSessionKey}
              ></sc-chat-view>`
            : ""}
          ${this.tab === "agents"
            ? html`<sc-agents-view></sc-agents-view>`
            : ""}
          ${this.tab === "sessions"
            ? html`<sc-sessions-view></sc-sessions-view>`
            : ""}
          ${this.tab === "models"
            ? html`<sc-models-view></sc-models-view>`
            : ""}
          ${this.tab === "config"
            ? html`<sc-config-view></sc-config-view>`
            : ""}
          ${this.tab === "tools" ? html`<sc-tools-view></sc-tools-view>` : ""}
          ${this.tab === "channels"
            ? html`<sc-channels-view></sc-channels-view>`
            : ""}
          ${this.tab === "cron" ? html`<sc-cron-view></sc-cron-view>` : ""}
          ${this.tab === "skills"
            ? html`<sc-skills-view></sc-skills-view>`
            : ""}
          ${this.tab === "voice" ? html`<sc-voice-view></sc-voice-view>` : ""}
          ${this.tab === "nodes" ? html`<sc-nodes-view></sc-nodes-view>` : ""}
          ${this.tab === "usage" ? html`<sc-usage-view></sc-usage-view>` : ""}
          ${this.tab === "logs" ? html`<sc-logs-view></sc-logs-view>` : ""}
        </main>
        <sc-floating-mic></sc-floating-mic>
      </div>
    `;
  }
}
