import { LitElement, html, css } from "lit";
import { customElement, state } from "lit/decorators.js";
import type { GatewayClient } from "../gateway.js";

interface ChannelStatus {
  name?: string;
  status?: string;
  healthy?: boolean;
  configured?: boolean;
  error?: string;
  [key: string]: unknown;
}

function unwrapPayload(res: unknown): unknown {
  const r = res as { payload?: unknown };
  return r?.payload ?? res;
}

@customElement("sc-channels-view")
export class ScChannelsView extends LitElement {
  static override styles = css`
    :host {
      display: block;
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(260px, 1fr));
      gap: 1rem;
    }
    .card {
      background: var(--sc-bg-surface);
      border: 1px solid var(--sc-border);
      border-radius: var(--sc-radius);
      padding: 1rem;
    }
    .card-header {
      display: flex;
      align-items: center;
      gap: 0.5rem;
      margin-bottom: 0.5rem;
    }
    .card-name {
      font-weight: 600;
      font-size: 1rem;
      color: var(--sc-text);
    }
    .status-dot {
      width: 8px;
      height: 8px;
      border-radius: 50%;
    }
    .status-dot.healthy {
      background: #22c55e;
    }
    .status-dot.error {
      background: #ef4444;
    }
    .status-dot.unconfigured {
      background: var(--sc-text-muted);
    }
    .card-info {
      font-size: 0.8125rem;
      color: var(--sc-text-muted);
    }
    .card-info .error {
      color: #ef4444;
    }
  `;

  @state() private channels: ChannelStatus[] = [];

  private gateway: GatewayClient | null = null;

  override connectedCallback(): void {
    super.connectedCallback();
    this.gateway =
      (document.querySelector("sc-app") as { gateway?: GatewayClient })
        ?.gateway ?? null;
    this.loadChannels();
  }

  private async loadChannels(): Promise<void> {
    if (!this.gateway) return;
    try {
      const res = await this.gateway.request("channels.status", {});
      const payload = unwrapPayload(res) as { channels?: ChannelStatus[] };
      this.channels = payload?.channels ?? [];
    } catch {
      this.channels = [];
    }
  }

  private dotClass(ch: ChannelStatus): string {
    if (ch.healthy === true) return "healthy";
    if (ch.configured === false || ch.status === "unconfigured")
      return "unconfigured";
    return "error";
  }

  override render() {
    return html`
      <div class="grid">
        ${this.channels.length === 0
          ? html`<p style="color: var(--sc-text-muted)">No channels</p>`
          : this.channels.map(
              (ch) => html`
                <div class="card">
                  <div class="card-header">
                    <span class="status-dot ${this.dotClass(ch)}"></span>
                    <span class="card-name">${ch.name ?? "unnamed"}</span>
                  </div>
                  <div class="card-info">
                    ${ch.error
                      ? html`<span class="error">${ch.error}</span>`
                      : (ch.status ?? "—")}
                  </div>
                </div>
              `,
            )}
      </div>
    `;
  }
}
