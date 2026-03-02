import { LitElement, html, css } from "lit";
import { customElement, state } from "lit/decorators.js";
import type { GatewayClient } from "../gateway.js";

interface UsageSummary {
  session_cost_usd?: number;
  daily_cost_usd?: number;
  monthly_cost_usd?: number;
  total_tokens?: number;
  request_count?: number;
}

@customElement("sc-usage-view")
export class ScUsageView extends LitElement {
  static override styles = css`
    :host {
      display: block;
      color: var(--sc-text);
    }
    h2 {
      margin: 0 0 1rem;
      font-size: 1.25rem;
      font-weight: 600;
      color: var(--sc-text);
    }
    .cards {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(180px, 1fr));
      gap: 1rem;
      margin-bottom: 1.5rem;
    }
    .card {
      padding: 1rem;
      background: var(--sc-bg-surface);
      border: 1px solid var(--sc-border);
      border-radius: var(--sc-radius);
    }
    .card-label {
      font-size: 0.75rem;
      color: var(--sc-text-muted);
      margin-bottom: 0.25rem;
    }
    .card-value {
      font-size: 1.25rem;
      font-weight: 600;
      color: var(--sc-text);
      font-variant-numeric: tabular-nums;
    }
    .chart-section {
      margin-top: 1.5rem;
    }
    .chart-title {
      font-size: 0.875rem;
      color: var(--sc-text-muted);
      margin-bottom: 0.5rem;
    }
    .bar-chart {
      display: flex;
      flex-direction: column;
      gap: 0.5rem;
    }
    .bar-row {
      display: flex;
      align-items: center;
      gap: 0.75rem;
    }
    .bar-label {
      width: 120px;
      font-size: 0.875rem;
      color: var(--sc-text-muted);
      flex-shrink: 0;
    }
    .bar-track {
      flex: 1;
      height: 20px;
      background: var(--sc-bg-elevated);
      border-radius: 4px;
      overflow: hidden;
    }
    .bar-fill {
      height: 100%;
      background: var(--sc-accent);
      border-radius: 4px;
      transition: width 0.3s ease;
    }
    .bar-value {
      width: 80px;
      font-size: 0.875rem;
      text-align: right;
      font-variant-numeric: tabular-nums;
    }
    .error {
      color: #f87171;
      font-size: 0.875rem;
    }
  `;

  @state() private summary: UsageSummary = {};
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
    this.loadSummary();
  }

  private async loadSummary(): Promise<void> {
    const gw = this.gateway;
    if (!gw) return;
    this.loading = true;
    this.error = "";
    try {
      const res = (await gw.request<UsageSummary>("usage.summary", {})) as
        | UsageSummary
        | { result?: UsageSummary };
      this.summary =
        (res && "result" in res && res.result) ||
        (res && "session_cost_usd" in res ? (res as UsageSummary) : {}) ||
        {};
    } catch (e) {
      this.error = e instanceof Error ? e.message : "Failed to load usage";
      this.summary = {};
    } finally {
      this.loading = false;
    }
  }

  private formatCurrency(v: number | undefined): string {
    if (v == null || Number.isNaN(v)) return "$0.00";
    return new Intl.NumberFormat("en-US", {
      style: "currency",
      currency: "USD",
      minimumFractionDigits: 2,
    }).format(v);
  }

  private formatNumber(v: number | undefined): string {
    if (v == null || Number.isNaN(v)) return "0";
    return new Intl.NumberFormat("en-US").format(v);
  }

  private barPct(value: number, max: number): number {
    if (max <= 0) return 0;
    return Math.min(100, (value / max) * 100);
  }

  override render() {
    const s = this.summary;
    const sessionCost = s.session_cost_usd ?? 0;
    const dailyCost = s.daily_cost_usd ?? 0;
    const monthlyCost = s.monthly_cost_usd ?? 0;
    const totalTokens = s.total_tokens ?? 0;
    const requestCount = s.request_count ?? 0;
    const maxCost = Math.max(sessionCost, dailyCost, monthlyCost, 0.01);

    return html`
      <h2>Usage</h2>
      ${this.error ? html`<p class="error">${this.error}</p>` : ""}
      ${this.loading
        ? html`<p style="color: var(--sc-text-muted)">Loading...</p>`
        : html`
            <div class="cards">
              <div class="card">
                <div class="card-label">Session Cost</div>
                <div class="card-value">
                  ${this.formatCurrency(sessionCost)}
                </div>
              </div>
              <div class="card">
                <div class="card-label">Daily Cost</div>
                <div class="card-value">${this.formatCurrency(dailyCost)}</div>
              </div>
              <div class="card">
                <div class="card-label">Monthly Cost</div>
                <div class="card-value">
                  ${this.formatCurrency(monthlyCost)}
                </div>
              </div>
              <div class="card">
                <div class="card-label">Total Tokens</div>
                <div class="card-value">${this.formatNumber(totalTokens)}</div>
              </div>
              <div class="card">
                <div class="card-label">Request Count</div>
                <div class="card-value">${this.formatNumber(requestCount)}</div>
              </div>
            </div>

            <div class="chart-section">
              <div class="chart-title">Cost comparison (bar chart)</div>
              <div class="bar-chart">
                <div class="bar-row">
                  <span class="bar-label">Session</span>
                  <div class="bar-track">
                    <div
                      class="bar-fill"
                      style="width: ${this.barPct(sessionCost, maxCost)}%"
                    ></div>
                  </div>
                  <span class="bar-value"
                    >${this.formatCurrency(sessionCost)}</span
                  >
                </div>
                <div class="bar-row">
                  <span class="bar-label">Daily</span>
                  <div class="bar-track">
                    <div
                      class="bar-fill"
                      style="width: ${this.barPct(dailyCost, maxCost)}%"
                    ></div>
                  </div>
                  <span class="bar-value"
                    >${this.formatCurrency(dailyCost)}</span
                  >
                </div>
                <div class="bar-row">
                  <span class="bar-label">Monthly</span>
                  <div class="bar-track">
                    <div
                      class="bar-fill"
                      style="width: ${this.barPct(monthlyCost, maxCost)}%"
                    ></div>
                  </div>
                  <span class="bar-value"
                    >${this.formatCurrency(monthlyCost)}</span
                  >
                </div>
              </div>
            </div>
          `}
    `;
  }
}
