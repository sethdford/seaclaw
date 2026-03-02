import { LitElement, html, css, nothing } from "lit";
import { customElement, state } from "lit/decorators.js";
import type { GatewayClient } from "../gateway.js";

interface NodeItem {
  id?: string;
  type?: string;
  status?: string;
  ws_connections?: number;
}

function unwrapPayload(res: unknown): unknown {
  const r = res as { payload?: unknown };
  return r?.payload ?? res;
}

@customElement("sc-nodes-view")
export class ScNodesView extends LitElement {
  static override styles = css`
    :host {
      display: block;
      color: var(--sc-text);
    }
    .header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-bottom: 1rem;
      flex-wrap: wrap;
      gap: 0.5rem;
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
      font-size: 0.875rem;
      cursor: pointer;
    }
    .refresh-btn:hover {
      background: var(--sc-border);
    }
    .refresh-btn:disabled {
      opacity: 0.5;
      cursor: not-allowed;
    }
    .nodes-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(220px, 1fr));
      gap: 1rem;
      margin-bottom: 1.5rem;
    }
    .node-card {
      padding: 1rem;
      background: var(--sc-bg-surface);
      border: 1px solid var(--sc-border);
      border-radius: var(--sc-radius);
    }
    .node-header {
      display: flex;
      align-items: center;
      gap: 0.5rem;
      margin-bottom: 0.5rem;
    }
    .status-dot {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      flex-shrink: 0;
    }
    .status-dot.green {
      background: #22c55e;
    }
    .status-dot.yellow {
      background: #eab308;
    }
    .status-dot.red {
      background: #ef4444;
    }
    .node-id {
      font-family: var(--sc-font-mono);
      font-size: 0.875rem;
      font-weight: 600;
      color: var(--sc-text);
      word-break: break-all;
    }
    .node-type {
      display: inline-block;
      padding: 0.2rem 0.5rem;
      font-size: 0.7rem;
      font-weight: 500;
      text-transform: lowercase;
      background: var(--sc-bg-elevated);
      color: var(--sc-text-muted);
      border-radius: 4px;
      margin-bottom: 0.5rem;
    }
    .node-info {
      font-size: 0.8125rem;
      color: var(--sc-text-muted);
    }
    .health-section {
      padding: 1rem;
      background: var(--sc-bg-surface);
      border: 1px solid var(--sc-border);
      border-radius: var(--sc-radius);
      margin-top: 1rem;
    }
    .health-title {
      font-size: 0.875rem;
      font-weight: 600;
      color: var(--sc-text);
      margin-bottom: 0.5rem;
    }
    .health-status {
      font-size: 0.8125rem;
      color: var(--sc-text-muted);
    }
    .health-status.ok {
      color: #22c55e;
    }
    .loading {
      color: var(--sc-text-muted);
      font-size: 0.875rem;
    }
    .empty {
      color: var(--sc-text-muted);
      font-size: 0.875rem;
      padding: 2rem;
      text-align: center;
      background: var(--sc-bg-surface);
      border: 1px dashed var(--sc-border);
      border-radius: var(--sc-radius);
    }
    .error {
      color: #ef4444;
      font-size: 0.875rem;
      margin-bottom: 1rem;
    }
  `;

  @state() private nodes: NodeItem[] = [];
  @state() private healthStatus = "";
  @state() private loading = false;
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
    if (!gw) return;
    this.loading = true;
    this.error = "";
    try {
      const [nodesRes, healthRes] = await Promise.all([
        gw.request("nodes.list", {}),
        gw.request("health", {}),
      ]);
      const nodesPayload = unwrapPayload(nodesRes) as { nodes?: NodeItem[] };
      this.nodes = nodesPayload?.nodes ?? [];
      const healthPayload = unwrapPayload(healthRes) as { status?: string };
      this.healthStatus = healthPayload?.status ?? "unknown";
    } catch (e) {
      this.error = e instanceof Error ? e.message : "Failed to load";
      this.nodes = [];
      this.healthStatus = "";
    } finally {
      this.loading = false;
    }
  }

  private statusDotClass(status: string | undefined): string {
    const s = (status ?? "").toLowerCase();
    if (s === "ok" || s === "healthy" || s === "connected" || s === "online")
      return "green";
    if (s === "degraded" || s === "warning") return "yellow";
    return "red";
  }

  private typeLabel(type: string | undefined): string {
    const t = (type ?? "").toLowerCase();
    if (t === "gateway" || t === "device" || t === "peripheral") return t;
    return t || "unknown";
  }

  override render() {
    return html`
      <div class="header">
        <h2>Nodes & Devices</h2>
        <button
          class="refresh-btn"
          ?disabled=${this.loading}
          @click=${() => this.load()}
        >
          ${this.loading ? "Refreshing..." : "Refresh"}
        </button>
      </div>

      ${this.error ? html`<p class="error">${this.error}</p>` : ""}
      ${this.loading && this.nodes.length === 0
        ? html`<p class="loading">Loading...</p>`
        : this.nodes.length === 0
          ? html`<div class="empty">No nodes found</div>`
          : html`
              <div class="nodes-grid">
                ${this.nodes.map(
                  (n) => html`
                    <div class="node-card">
                      <div class="node-header">
                        <span
                          class="status-dot ${this.statusDotClass(n.status)}"
                        ></span>
                        <span class="node-id">${n.id ?? "—"}</span>
                      </div>
                      <div class="node-type">${this.typeLabel(n.type)}</div>
                      <div class="node-info">
                        WebSocket connections: ${n.ws_connections ?? 0}
                      </div>
                    </div>
                  `,
                )}
              </div>
            `}

      <div class="health-section">
        <div class="health-title">Gateway health</div>
        <div class="health-status ${this.healthStatus === "ok" ? "ok" : ""}">
          ${this.loading && !this.healthStatus
            ? "Loading..."
            : this.healthStatus || "—"}
        </div>
      </div>
    `;
  }
}
