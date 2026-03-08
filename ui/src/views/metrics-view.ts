import { html, css, nothing } from "lit";
import { customElement, state } from "lit/decorators.js";
import { GatewayAwareLitElement } from "../gateway-aware.js";
import "../components/sc-page-hero.js";
import "../components/sc-section-header.js";
import "../components/sc-stat-card.js";
import "../components/sc-stats-row.js";
import "../components/sc-card.js";
import "../components/sc-skeleton.js";
import "../components/sc-empty-state.js";
import "../components/sc-button.js";
import { icons } from "../icons.js";

interface MetricsSnapshot {
  health?: { uptime_seconds?: number; pid?: number };
  metrics?: {
    total_requests?: number;
    total_tokens?: number;
    total_tool_calls?: number;
    total_errors?: number;
    avg_latency_ms?: number;
    active_sessions?: number;
  };
  bth?: Record<string, number>;
}

function formatUptime(seconds: number): string {
  const d = Math.floor(seconds / 86400);
  const h = Math.floor((seconds % 86400) / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  const parts: string[] = [];
  if (d > 0) parts.push(`${d}d`);
  if (h > 0) parts.push(`${h}h`);
  parts.push(`${m}m`);
  return parts.join(" ");
}

const BTH_GROUPS: Record<string, string[]> = {
  Memory: [
    "emotions_surfaced",
    "facts_extracted",
    "emotions_promoted",
    "replay_analyses",
    "egraph_contexts",
  ],
  Conversation: [
    "pattern_insights",
    "mood_contexts_built",
    "thinking_responses",
    "ab_evaluations",
    "ab_alternates_chosen",
  ],
  Engagement: [
    "silence_checkins",
    "event_followups",
    "starters_built",
    "callbacks_triggered",
    "reactions_sent",
  ],
  Context: [
    "link_contexts",
    "attachment_contexts",
    "vision_descriptions",
    "commitment_followups",
    "events_extracted",
  ],
  Polish: ["typos_applied", "corrections_sent"],
};

function labelFromKey(key: string): string {
  return key.replace(/_/g, " ").replace(/\b\w/g, (c) => c.toUpperCase());
}

@customElement("sc-metrics-view")
export class ScMetricsView extends GatewayAwareLitElement {
  override autoRefreshInterval = 10_000;

  static override styles = css`
    :host {
      view-transition-name: view-metrics;
      display: block;
      color: var(--sc-text);
      max-width: 60rem;
    }
    .section {
      margin-bottom: var(--sc-space-2xl);
    }
    .metric-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(10rem, 1fr));
      gap: var(--sc-space-md);
    }
    .metric-item {
      display: flex;
      flex-direction: column;
      gap: var(--sc-space-2xs);
    }
    .metric-label {
      font-size: var(--sc-text-xs);
      color: var(--sc-text-muted);
      text-transform: uppercase;
      letter-spacing: 0.04em;
    }
    .metric-value {
      font-family: var(--sc-font-mono);
      font-size: var(--sc-text-base);
      font-weight: var(--sc-weight-semibold);
      color: var(--sc-text);
    }
    .bth-group {
      margin-bottom: var(--sc-space-xl);
    }
    .bth-group:last-child {
      margin-bottom: 0;
    }
    .card-inner {
      padding: var(--sc-space-md);
    }
  `;

  @state() private snapshot: MetricsSnapshot = {};
  @state() private loading = false;
  @state() private error = "";

  protected override async load(): Promise<void> {
    const gw = this.gateway;
    if (!gw) return;
    this.loading = true;
    this.error = "";
    try {
      const res = (await gw.request<MetricsSnapshot>("metrics.snapshot", {})) as
        | MetricsSnapshot
        | { result?: MetricsSnapshot };
      this.snapshot =
        (res && "result" in res && res.result) ||
        (res && "health" in res ? (res as MetricsSnapshot) : {}) ||
        {};
    } catch (e) {
      this.error = e instanceof Error ? e.message : "Failed to load metrics";
      this.snapshot = {};
    } finally {
      this.loading = false;
    }
  }

  private _renderSkeleton() {
    return html`
      <sc-stats-row>
        <sc-skeleton variant="card" height="90px"></sc-skeleton>
        <sc-skeleton variant="card" height="90px"></sc-skeleton>
        <sc-skeleton variant="card" height="90px"></sc-skeleton>
        <sc-skeleton variant="card" height="90px"></sc-skeleton>
        <sc-skeleton variant="card" height="90px"></sc-skeleton>
      </sc-stats-row>
      <sc-skeleton variant="card" height="180px"></sc-skeleton>
      <sc-skeleton variant="card" height="280px"></sc-skeleton>
    `;
  }

  private _renderSystemHealth() {
    const m = this.snapshot.metrics;
    if (!m) return nothing;

    const items = [
      { key: "total_requests", label: "Total Requests", value: m.total_requests },
      { key: "total_tokens", label: "Total Tokens", value: m.total_tokens },
      { key: "total_tool_calls", label: "Tool Calls", value: m.total_tool_calls },
      { key: "total_errors", label: "Errors", value: m.total_errors },
      { key: "avg_latency_ms", label: "Avg Latency (ms)", value: m.avg_latency_ms },
      { key: "active_sessions", label: "Active Sessions", value: m.active_sessions },
    ];

    return html`
      <div class="section" role="region" aria-label="System health metrics">
        <sc-section-header
          heading="System Health"
          description="Gateway and request metrics"
        ></sc-section-header>
        <sc-card glass>
          <div class="card-inner">
            <div class="metric-grid">
              ${items.map(
                (item) => html`
                  <div class="metric-item">
                    <span class="metric-label">${item.label}</span>
                    <span class="metric-value"
                      >${item.value != null
                        ? typeof item.value === "number" && item.value >= 1000
                          ? (item.value as number).toLocaleString()
                          : String(item.value)
                        : "—"}</span
                    >
                  </div>
                `,
              )}
            </div>
          </div>
        </sc-card>
      </div>
    `;
  }

  private _renderIntelligencePipeline() {
    const bth = this.snapshot.bth;
    if (!bth) return nothing;

    return html`
      <div class="section" role="region" aria-label="Intelligence pipeline metrics">
        <sc-section-header
          heading="Intelligence Pipeline"
          description="BTH pipeline metrics by category"
        ></sc-section-header>
        <sc-card glass>
          <div class="card-inner">
            ${Object.entries(BTH_GROUPS).map(
              ([groupName, keys]) => html`
                <div class="bth-group">
                  <div class="metric-label" style="margin-bottom: var(--sc-space-sm)">
                    ${groupName}
                  </div>
                  <div class="metric-grid">
                    ${keys.map((key) => {
                      const val = bth[key];
                      return html`
                        <div class="metric-item">
                          <span class="metric-label">${labelFromKey(key)}</span>
                          <span class="metric-value"
                            >${val != null ? (val as number).toLocaleString() : "—"}</span
                          >
                        </div>
                      `;
                    })}
                  </div>
                </div>
              `,
            )}
          </div>
        </sc-card>
      </div>
    `;
  }

  override render() {
    const h = this.snapshot.health;
    const m = this.snapshot.metrics;
    const bth = this.snapshot.bth;
    const uptime = h?.uptime_seconds ?? 0;
    const activeSessions = m?.active_sessions ?? 0;
    const avgLatency = m?.avg_latency_ms ?? 0;
    const totalRequests = m?.total_requests ?? 0;
    const totalErrors = m?.total_errors ?? 0;
    const errorRate = totalRequests > 0 ? ((totalErrors / totalRequests) * 100).toFixed(2) : "0";
    const totalTurns = bth?.total_turns ?? m?.total_requests ?? 0;

    return html`
      <sc-page-hero role="region" aria-label="Observability">
        <sc-section-header
          heading="Observability"
          description="Live system health and intelligence metrics"
        >
        </sc-section-header>
      </sc-page-hero>

      <sc-stats-row>
        <sc-stat-card
          .valueStr=${formatUptime(uptime)}
          label="Uptime"
          style="--sc-stagger-delay: 0ms"
        ></sc-stat-card>
        <sc-stat-card
          .value=${activeSessions}
          label="Active Sessions"
          style="--sc-stagger-delay: 50ms"
        ></sc-stat-card>
        <sc-stat-card
          .value=${avgLatency}
          label="Avg Latency (ms)"
          style="--sc-stagger-delay: 100ms"
        ></sc-stat-card>
        <sc-stat-card
          .valueStr="${errorRate}%"
          label="Error Rate"
          style="--sc-stagger-delay: 150ms"
        ></sc-stat-card>
        <sc-stat-card
          .value=${totalTurns}
          label="Total Turns"
          style="--sc-stagger-delay: 200ms"
        ></sc-stat-card>
      </sc-stats-row>

      ${this.error
        ? html`<sc-empty-state .icon=${icons.warning} heading="Error" description=${this.error}>
            <sc-button
              variant="primary"
              @click=${() => this.load()}
              aria-label="Retry loading metrics"
              >Retry</sc-button
            >
          </sc-empty-state>`
        : nothing}
      ${this.loading
        ? this._renderSkeleton()
        : html` ${this._renderSystemHealth()} ${this._renderIntelligencePipeline()} `}
    `;
  }
}
