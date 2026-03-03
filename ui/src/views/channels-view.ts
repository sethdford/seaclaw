import { html, css, nothing } from "lit";
import { customElement, state } from "lit/decorators.js";
import { GatewayAwareLitElement } from "../gateway-aware.js";
import { icons } from "../icons.js";
import "../components/sc-card.js";
import "../components/sc-skeleton.js";
import "../components/sc-empty-state.js";

interface ChannelStatus {
  key?: string;
  label?: string;
  name?: string;
  status?: string;
  healthy?: boolean;
  configured?: boolean;
  build_enabled?: boolean;
  error?: string;
}

@customElement("sc-channels-view")
export class ScChannelsView extends GatewayAwareLitElement {
  static override styles = css`
    :host {
      display: block;
      max-width: 1200px;
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
      gap: var(--sc-space-xl);
    }
    .grid-full {
      grid-column: 1 / -1;
    }
    .card-header {
      display: flex;
      align-items: center;
      gap: var(--sc-space-sm);
      margin-bottom: var(--sc-space-sm);
    }
    .card-name {
      font-weight: var(--sc-weight-semibold);
      font-size: var(--sc-text-lg);
      color: var(--sc-text);
    }
    .status-dot {
      display: inline-block;
      width: 8px;
      height: 8px;
      border-radius: 50%;
      margin-right: var(--sc-space-xs);
    }
    .status-dot.healthy {
      background: var(--sc-success);
    }
    .status-dot.error {
      background: var(--sc-error);
    }
    .status-dot.unconfigured {
      background: var(--sc-text-faint);
    }
    .card-info {
      font-size: var(--sc-text-sm);
      color: var(--sc-text-muted);
    }
    .card-info .error {
      color: var(--sc-error);
    }
    @media (max-width: 768px) {
      .grid {
        grid-template-columns: 1fr 1fr;
      }
    }
    @media (max-width: 480px) {
      .grid {
        grid-template-columns: 1fr;
      }
    }
  `;

  @state() private channels: ChannelStatus[] = [];
  @state() private loading = true;
  @state() private error = "";

  protected override async load(): Promise<void> {
    await this.loadChannels();
  }

  private async loadChannels(): Promise<void> {
    if (!this.gateway) {
      this.loading = false;
      return;
    }
    this.loading = true;
    try {
      const payload = await this.gateway.request<{
        channels?: ChannelStatus[];
      }>("channels.status", {});
      this.channels = payload?.channels ?? [];
    } catch (e) {
      this.channels = [];
      this.error = e instanceof Error ? e.message : "Failed to load channels";
    } finally {
      this.loading = false;
    }
  }

  private dotClass(ch: ChannelStatus): string {
    if (ch.healthy === true || ch.configured === true) return "healthy";
    if (ch.build_enabled === false) return "unconfigured";
    if (ch.configured === false || ch.status === "unconfigured") return "unconfigured";
    return "error";
  }

  override render() {
    if (this.loading) {
      return html`
        <div class="grid sc-stagger">
          <sc-skeleton variant="card" height="80px"></sc-skeleton>
          <sc-skeleton variant="card" height="80px"></sc-skeleton>
          <sc-skeleton variant="card" height="80px"></sc-skeleton>
        </div>
      `;
    }

    return html`
      ${this.error
        ? html`<sc-empty-state
            .icon=${icons.warning}
            heading="Error"
            description=${this.error}
          ></sc-empty-state>`
        : nothing}
      <div class="grid sc-stagger">
        ${this.channels.length === 0
          ? html`
              <div class="grid-full">
                <sc-empty-state
                  .icon=${icons.radio}
                  heading="No channels configured"
                  description="Configure messaging channels to receive and send messages."
                ></sc-empty-state>
              </div>
            `
          : this.channels.map(
              (ch) => html`
                <sc-card>
                  <div class="card-header">
                    <span class="status-dot ${this.dotClass(ch)}"></span>
                    <span class="card-name">${ch.label || ch.key || ch.name || "unnamed"}</span>
                  </div>
                  <div class="card-info">
                    ${ch.error ? html`<span class="error">${ch.error}</span>` : (ch.status ?? "—")}
                  </div>
                </sc-card>
              `,
            )}
      </div>
    `;
  }
}
