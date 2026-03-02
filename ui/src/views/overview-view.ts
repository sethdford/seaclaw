import { LitElement, html, css, nothing } from "lit";
import { customElement, state } from "lit/decorators.js";
import type { GatewayClient } from "../gateway.js";

interface HealthRes {
  status?: string;
}

interface CapabilitiesRes {
  version?: string;
  tools?: number;
  channels?: number;
  providers?: number;
}

interface ChannelItem {
  key?: string;
  label?: string;
  configured?: boolean;
  status?: string;
  build_enabled?: boolean;
}

interface SessionItem {
  key?: string;
  label?: string;
  created_at?: number;
  last_active?: number;
  turn_count?: number;
}

function unwrapPayload(res: unknown): unknown {
  const r = res as { payload?: unknown; result?: unknown };
  return r?.payload ?? r?.result ?? res;
}

@customElement("sc-overview-view")
export class ScOverviewView extends LitElement {
  static override styles = css`
    :host {
      display: block;
    }
    .header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-bottom: 1.5rem;
      flex-wrap: wrap;
      gap: 1rem;
    }
    h2 {
      margin: 0;
      font-size: 1.25rem;
      font-weight: 600;
      color: var(--sc-text);
    }
    .refresh-btn {
      padding: 0.5rem 1rem;
      background: var(--sc-bg-elevated);
      color: var(--sc-text);
      border: 1px solid var(--sc-border);
      border-radius: var(--sc-radius);
      cursor: pointer;
      font-size: 0.875rem;
    }
    .refresh-btn:hover {
      background: var(--sc-border);
    }
    .status-cards {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(140px, 1fr));
      gap: 0.75rem;
      margin-bottom: 1.5rem;
    }
    .status-card {
      padding: 0.75rem 1rem;
      background: var(--sc-bg-surface);
      border: 1px solid var(--sc-border);
      border-radius: var(--sc-radius);
    }
    .status-card-label {
      font-size: 0.75rem;
      color: var(--sc-text-muted);
      margin-bottom: 0.25rem;
    }
    .status-card-value {
      font-size: 1rem;
      font-weight: 600;
      color: var(--sc-text);
      display: flex;
      align-items: center;
      gap: 0.5rem;
    }
    .status-dot {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      flex-shrink: 0;
    }
    .status-dot.operational {
      background: #22c55e;
    }
    .status-dot.offline {
      background: #ef4444;
    }
    .section-title {
      font-size: 0.875rem;
      font-weight: 600;
      color: var(--sc-text);
      margin-bottom: 0.75rem;
    }
    .channels-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));
      gap: 0.75rem;
      margin-bottom: 1.5rem;
    }
    .channel-card {
      padding: 0.75rem 1rem;
      background: var(--sc-bg-surface);
      border: 1px solid var(--sc-border);
      border-radius: var(--sc-radius);
    }
    .channel-card-header {
      display: flex;
      align-items: center;
      gap: 0.5rem;
      margin-bottom: 0.25rem;
    }
    .channel-dot {
      width: 6px;
      height: 6px;
      border-radius: 50%;
      flex-shrink: 0;
    }
    .channel-dot.configured {
      background: #22c55e;
    }
    .channel-dot.unconfigured {
      background: var(--sc-text-muted);
    }
    .channel-name {
      font-weight: 500;
      font-size: 0.875rem;
      color: var(--sc-text);
    }
    .channel-status {
      font-size: 0.75rem;
      color: var(--sc-text-muted);
    }
    .sessions-list {
      background: var(--sc-bg-surface);
      border: 1px solid var(--sc-border);
      border-radius: var(--sc-radius);
      overflow: hidden;
    }
    .session-row {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 0.5rem 1rem;
      border-bottom: 1px solid var(--sc-border);
      font-size: 0.875rem;
    }
    .session-row:last-child {
      border-bottom: none;
    }
    .session-info {
      color: var(--sc-text);
    }
    .session-meta {
      font-size: 0.75rem;
      color: var(--sc-text-muted);
    }
    .loading {
      color: var(--sc-text-muted);
      font-size: 0.875rem;
    }
    .error {
      color: #ef4444;
      font-size: 0.875rem;
      margin-bottom: 1rem;
    }
  `;

  @state() private health: HealthRes = {};
  @state() private capabilities: CapabilitiesRes = {};
  @state() private channels: ChannelItem[] = [];
  @state() private sessions: SessionItem[] = [];
  @state() private loading = true;
  @state() private error = "";

  private get gateway(): GatewayClient | null {
    return (
      (document.querySelector("sc-app") as { gateway?: GatewayClient })
        ?.gateway ?? null
    );
  }

  override connectedCallback(): void {
    super.connectedCallback();
    this.load();
  }

  private async load(): Promise<void> {
    const gw = this.gateway;
    if (!gw) {
      this.loading = false;
      this.error = "Not connected";
      return;
    }
    this.loading = true;
    this.error = "";
    try {
      const [healthRes, capRes, chRes, sessRes] = await Promise.all([
        gw.request<HealthRes>("health", {}).catch(() => ({})),
        gw.request<CapabilitiesRes>("capabilities", {}).catch(() => ({})),
        gw
          .request<{ channels?: ChannelItem[] }>("channels.status", {})
          .catch(() => ({ channels: [] })),
        gw
          .request<{ sessions?: SessionItem[] }>("sessions.list", {})
          .catch(() => ({ sessions: [] })),
      ]);
      this.health = unwrapPayload(healthRes) as HealthRes;
      this.capabilities = unwrapPayload(capRes) as CapabilitiesRes;
      const chPayload = unwrapPayload(chRes) as { channels?: ChannelItem[] };
      this.channels = chPayload?.channels ?? [];
      const sessPayload = unwrapPayload(sessRes) as {
        sessions?: SessionItem[];
      };
      this.sessions = sessPayload?.sessions ?? [];
    } catch (e) {
      this.error = e instanceof Error ? e.message : "Failed to load overview";
      this.health = {};
      this.capabilities = {};
      this.channels = [];
      this.sessions = [];
    } finally {
      this.loading = false;
    }
  }

  private get gatewayOperational(): boolean {
    const s = (this.health.status ?? "").toLowerCase();
    return s === "ok" || s === "operational" || s === "healthy";
  }

  private get recentSessions(): SessionItem[] {
    const sorted = [...this.sessions].sort((a, b) => {
      const aTs = a.last_active ?? a.created_at ?? 0;
      const bTs = b.last_active ?? b.created_at ?? 0;
      return (bTs as number) - (aTs as number);
    });
    return sorted.slice(0, 5);
  }

  private formatDate(v?: number | string): string {
    if (v == null) return "—";
    try {
      const ts = typeof v === "number" ? (v < 1e12 ? v * 1000 : v) : v;
      return new Intl.DateTimeFormat(undefined, {
        dateStyle: "short",
        timeStyle: "short",
      }).format(new Date(ts));
    } catch {
      return String(v);
    }
  }

  override render() {
    if (this.loading) {
      return html`
        <div class="header">
          <h2>Overview</h2>
        </div>
        <p class="loading">Loading...</p>
      `;
    }

    const cap = this.capabilities;
    const gwOk = this.gatewayOperational;

    return html`
      <div class="header">
        <h2>Overview</h2>
        <button class="refresh-btn" @click=${() => this.load()}>Refresh</button>
      </div>
      ${this.error ? html`<p class="error">${this.error}</p>` : nothing}
      <div class="status-cards">
        <div class="status-card">
          <div class="status-card-label">Gateway</div>
          <div class="status-card-value">
            <span class="status-dot ${gwOk ? "operational" : "offline"}"></span>
            ${gwOk ? "Operational" : "Offline"}
          </div>
        </div>
        <div class="status-card">
          <div class="status-card-label">Version</div>
          <div class="status-card-value">${cap.version ?? "—"}</div>
        </div>
        <div class="status-card">
          <div class="status-card-label">Providers</div>
          <div class="status-card-value">${cap.providers ?? 0}</div>
        </div>
        <div class="status-card">
          <div class="status-card-label">Channels</div>
          <div class="status-card-value">${cap.channels ?? 0}</div>
        </div>
        <div class="status-card">
          <div class="status-card-label">Tools</div>
          <div class="status-card-value">${cap.tools ?? 0}</div>
        </div>
        <div class="status-card">
          <div class="status-card-label">Active Sessions</div>
          <div class="status-card-value">${this.sessions.length}</div>
        </div>
      </div>
      <div class="section-title">Channels</div>
      <div class="channels-grid">
        ${this.channels.map(
          (ch) => html`
            <div class="channel-card">
              <div class="channel-card-header">
                <span
                  class="channel-dot ${ch.configured
                    ? "configured"
                    : "unconfigured"}"
                ></span>
                <span class="channel-name"
                  >${ch.label ?? ch.key ?? "unnamed"}</span
                >
              </div>
              <div class="channel-status">${ch.status ?? "—"}</div>
            </div>
          `,
        )}
      </div>
      <div class="section-title">Recent Sessions</div>
      <div class="sessions-list">
        ${this.recentSessions.length === 0
          ? html`<div class="session-row">
              <span class="session-meta">No sessions</span>
            </div>`
          : this.recentSessions.map(
              (s) => html`
                <div class="session-row">
                  <span class="session-info"
                    >${s.label ?? s.key ?? "unnamed"}</span
                  >
                  <span class="session-meta"
                    >${s.turn_count ?? 0} turns ·
                    ${this.formatDate(s.last_active)}</span
                  >
                </div>
              `,
            )}
      </div>
    `;
  }
}
