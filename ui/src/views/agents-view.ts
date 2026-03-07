import { html, css, nothing } from "lit";
import { customElement, state } from "lit/decorators.js";
import { formatRelative } from "../utils.js";
import { GatewayAwareLitElement } from "../gateway-aware.js";
import { icons } from "../icons.js";
import "../components/sc-card.js";
import "../components/sc-skeleton.js";
import "../components/sc-page-hero.js";
import "../components/sc-section-header.js";
import "../components/sc-stat-card.js";
import "../components/sc-empty-state.js";
import "../components/sc-button.js";
import "../components/sc-tooltip.js";

interface ConfigData {
  default_provider?: string;
  default_model?: string;
  temperature?: number;
  max_tokens?: number;
}

interface Session {
  key?: string;
  label?: string;
  turn_count?: number;
  last_active?: number;
  created_at?: number;
}

interface Capabilities {
  tools?: unknown[];
  channels?: unknown[];
  providers?: unknown[];
}

@customElement("sc-agents-view")
export class ScAgentsView extends GatewayAwareLitElement {
  override autoRefreshInterval = 30_000;

  static override styles = css`
    :host {
      display: block;
      color: var(--sc-text);
      max-width: 1200px;
      padding: var(--sc-space-lg) var(--sc-space-xl);
    }

    .staleness {
      font-size: var(--sc-text-xs);
      color: var(--sc-text-muted);
    }

    .metrics {
      display: grid;
      grid-template-columns: repeat(4, 1fr);
      gap: var(--sc-space-lg, 1.5rem);
      margin-bottom: var(--sc-space-2xl, 2rem);
    }

    /* ── Sessions zone ────────────────────────────────── */

    .section-header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-bottom: var(--sc-space-sm);
    }

    .section-title {
      font-size: var(--sc-text-lg);
      font-weight: var(--sc-weight-semibold);
      color: var(--sc-text);
    }

    .sessions-list {
      display: flex;
      flex-direction: column;
    }

    .session-row {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: var(--sc-space-md);
      flex-wrap: wrap;
      padding: var(--sc-space-sm) 0;
    }

    .session-divider {
      height: 1px;
      background: var(--sc-border);
    }

    .session-info {
      flex: 1;
      min-width: 0;
    }

    .session-key {
      font-family: var(--sc-font-mono);
      font-size: var(--sc-text-sm);
      color: var(--sc-text);
      margin-bottom: var(--sc-space-2xs);
    }

    .session-meta {
      font-size: var(--sc-text-xs);
      color: var(--sc-text-muted);
    }

    .card-spacer {
      margin-bottom: var(--sc-space-2xl);
    }

    /* ── Config zone ──────────────────────────────────── */

    .profile-header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-bottom: var(--sc-space-sm);
    }

    .profile-title {
      font-size: var(--sc-text-lg);
      font-weight: var(--sc-weight-semibold);
      color: var(--sc-text);
    }

    .profile-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(140px, 1fr));
      gap: var(--sc-space-sm);
      font-size: var(--sc-text-sm);
    }

    .profile-item {
      color: var(--sc-text-muted);
    }

    .profile-item strong {
      color: var(--sc-text);
    }

    .skeleton-metrics {
      display: grid;
      grid-template-columns: repeat(4, 1fr);
      gap: var(--sc-space-lg, 1.5rem);
      margin-bottom: var(--sc-space-2xl, 2rem);
    }

    .skeleton-sessions {
      margin-bottom: var(--sc-space-2xl);
    }

    .skeleton-lines {
      display: flex;
      flex-direction: column;
      gap: var(--sc-space-sm);
      padding: var(--sc-space-md);
    }

    /* ── Responsive ───────────────────────────────────── */

    @media (max-width: 768px) {
      .metrics,
      .skeleton-metrics {
        grid-template-columns: repeat(2, 1fr);
      }
      .profile-grid {
        grid-template-columns: 1fr 1fr;
      }
    }

    @media (max-width: 480px) {
      .metrics,
      .skeleton-metrics {
        grid-template-columns: 1fr;
      }
      .profile-grid {
        grid-template-columns: 1fr;
      }
    }
  `;

  @state() private config: ConfigData = {};
  @state() private sessions: Session[] = [];
  @state() private capabilities: Capabilities = {};
  @state() private loading = false;
  @state() private error = "";

  protected override async load(): Promise<void> {
    const gw = this.gateway;
    if (!gw) return;
    this.loading = true;
    this.error = "";
    try {
      const [cfg, sess, caps] = await Promise.all([
        gw.request<Partial<ConfigData>>("config.get", {}),
        gw.request<{ sessions?: Session[] }>("sessions.list", {}),
        gw.request<Capabilities>("capabilities", {}),
      ]);
      this.config = {
        default_provider: cfg?.default_provider ?? "",
        default_model: cfg?.default_model ?? "",
        temperature: cfg?.temperature ?? 0.7,
        max_tokens: cfg?.max_tokens ?? 0,
      };
      this.sessions = sess?.sessions ?? [];
      this.capabilities = {
        tools: caps?.tools ?? [],
        channels: caps?.channels ?? [],
        providers: caps?.providers ?? [],
      };
    } catch (e) {
      this.error = e instanceof Error ? e.message : "Failed to load";
    } finally {
      this.loading = false;
    }
  }

  private dispatchNavigate(tab: string): void {
    this.dispatchEvent(
      new CustomEvent("navigate", {
        detail: tab,
        bubbles: true,
        composed: true,
      }),
    );
  }

  private get totalTurns(): number {
    return this.sessions.reduce((s, x) => s + (x.turn_count ?? 0), 0);
  }

  private get _toolCount(): number {
    const t = this.capabilities.tools;
    return typeof t === "number" ? t : Array.isArray(t) ? t.length : 0;
  }

  private get _channelCount(): number {
    const ch = this.capabilities.channels;
    return typeof ch === "number" ? ch : Array.isArray(ch) ? ch.length : 0;
  }

  override render() {
    if (this.loading) return this._renderSkeleton();
    return html`
      ${this.error
        ? html`<sc-empty-state
            .icon=${icons.warning}
            heading="Error"
            description=${this.error}
          ></sc-empty-state>`
        : nothing}
      ${this._renderHero()} ${this._renderMetrics()} ${this._renderSessions()}
      ${this._renderConfig()}
    `;
  }

  /* ── Hero zone ──────────────────────────────────────── */

  private _renderHero() {
    const provider = this.config.default_provider || "\u2014";
    const model = this.config.default_model || "\u2014";
    return html`
      <sc-page-hero>
        <sc-section-header heading="SeaClaw Agent" description="${provider} · ${model}">
          <span class="staleness">${this.stalenessLabel}</span>
          <sc-button size="sm" @click=${() => this.load()} aria-label="Refresh data"
            >Refresh</sc-button
          >
        </sc-section-header>
      </sc-page-hero>
    `;
  }

  /* ── Metrics zone ───────────────────────────────────── */

  private _renderMetrics() {
    const metrics = [
      { label: "Sessions", value: this.sessions.length },
      { label: "Turns", value: this.totalTurns },
      { label: "Tools", value: this._toolCount },
      { label: "Channels", value: this._channelCount },
    ];

    return html`
      <div class="metrics">
        ${metrics.map(
          (m, i) => html`
            <sc-stat-card
              .value=${m.value}
              .label=${m.label}
              style="--sc-stagger-delay: ${i * 80}ms"
            ></sc-stat-card>
          `,
        )}
      </div>
    `;
  }

  /* ── Sessions zone ──────────────────────────────────── */

  private _renderSessions() {
    return html`
      <sc-card class="card-spacer">
        <div class="section-header">
          <span class="section-title">Active Sessions</span>
          <sc-button
            variant="primary"
            size="sm"
            @click=${() => this.dispatchNavigate("chat:default")}
            aria-label="Start new conversation"
          >
            + New Chat
          </sc-button>
        </div>
        ${this.sessions.length === 0
          ? html`
              <sc-empty-state
                .icon=${icons["chat-circle"]}
                heading="No active sessions"
                description="Start a conversation to see sessions here."
              ></sc-empty-state>
            `
          : html`
              <div class="sessions-list" role="list">
                ${this.sessions.map(
                  (s, idx) => html`
                    ${idx > 0 ? html`<div class="session-divider"></div>` : nothing}
                    <div class="session-row" role="listitem">
                      <div class="session-info">
                        <div class="session-key">${s.label ?? s.key ?? "\u2014"}</div>
                        <div class="session-meta">
                          ${s.turn_count ?? 0} turns &middot; ${formatRelative(s.last_active)}
                        </div>
                      </div>
                      <sc-button
                        variant="primary"
                        size="sm"
                        @click=${() => this.dispatchNavigate("chat:" + (s.key ?? "default"))}
                        aria-label="Resume session"
                      >
                        Resume
                      </sc-button>
                    </div>
                  `,
                )}
              </div>
            `}
      </sc-card>
    `;
  }

  /* ── Config zone ────────────────────────────────────── */

  private _renderConfig() {
    return html`
      <sc-card>
        <div class="profile-header">
          <span class="profile-title">Configuration</span>
          <sc-button
            size="sm"
            @click=${() => this.dispatchNavigate("config")}
            aria-label="Edit configuration"
          >
            Edit Config
          </sc-button>
        </div>
        <div class="profile-grid">
          <div class="profile-item">
            Provider: <strong>${this.config.default_provider ?? "\u2014"}</strong>
          </div>
          <div class="profile-item">
            Model: <strong>${this.config.default_model ?? "\u2014"}</strong>
          </div>
          <div class="profile-item">
            Temperature: <strong>${this.config.temperature ?? "\u2014"}</strong>
          </div>
          <div class="profile-item">
            Max tokens: <strong>${this.config.max_tokens ?? "\u2014"}</strong>
          </div>
        </div>
      </sc-card>
    `;
  }

  /* ── Skeleton ───────────────────────────────────────── */

  private _renderSkeleton() {
    return html`
      <sc-skeleton variant="card" height="90px"></sc-skeleton>
      <div class="skeleton-metrics">
        <sc-skeleton variant="stat-card"></sc-skeleton>
        <sc-skeleton variant="stat-card"></sc-skeleton>
        <sc-skeleton variant="stat-card"></sc-skeleton>
        <sc-skeleton variant="stat-card"></sc-skeleton>
      </div>
      <sc-skeleton variant="card" height="160px" class="skeleton-sessions"></sc-skeleton>
      <sc-skeleton variant="card" height="100px"></sc-skeleton>
    `;
  }
}
