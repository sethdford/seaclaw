import { html, css, nothing } from "lit";
import { customElement, state } from "lit/decorators.js";
import { GatewayAwareLitElement } from "../gateway-aware.js";
import { icons } from "../icons.js";
import type { DataTableColumnV2 } from "../components/sc-data-table-v2.js";
import "../components/sc-button.js";
import "../components/sc-data-table-v2.js";
import "../components/sc-json-viewer.js";
import "../components/sc-page-hero.js";
import "../components/sc-section-header.js";
import "../components/sc-stat-card.js";
import "../components/sc-stats-row.js";
import "../components/sc-skeleton.js";
import "../components/sc-empty-state.js";

function friendlyError(e: unknown): string {
  const msg = e instanceof Error ? e.message : String(e);
  if (msg.includes("timeout")) return "Request timed out. Please try again.";
  if (msg.includes("WebSocket")) return "Connection lost. Reconnecting...";
  if (msg.includes("404")) return "Resource not found.";
  if (msg.includes("401") || msg.includes("unauthorized"))
    return "Authentication failed. Please check your credentials.";
  if (msg.includes("403") || msg.includes("forbidden")) return "Access denied.";
  if (msg.includes("network")) return "Network error. Please check your connection.";
  return "Something went wrong. Please try again.";
}

interface ToolDef {
  name?: string;
  description?: string;
  parameters?: unknown;
}

function resolveParams(raw: unknown): Record<string, unknown> | null {
  if (raw == null) return null;
  if (typeof raw === "string") {
    try {
      const parsed = JSON.parse(raw);
      if (typeof parsed === "object" && parsed !== null) return parsed as Record<string, unknown>;
    } catch {
      return null;
    }
    return null;
  }
  if (typeof raw === "object") return raw as Record<string, unknown>;
  return null;
}

function countParams(raw: unknown): number {
  const obj = resolveParams(raw);
  if (!obj) return 0;
  const props = obj.properties;
  if (props && typeof props === "object" && !Array.isArray(props))
    return Object.keys(props as object).length;
  return Object.keys(obj).length;
}

@customElement("sc-tools-view")
export class ScToolsView extends GatewayAwareLitElement {
  static override styles = css`
    :host {
      view-transition-name: view-tools;
      display: block;
      max-width: 75rem;
    }
    .table-section {
      margin-top: var(--sc-space-xl);
    }
    .expand-panel {
      margin-top: var(--sc-space-md);
      padding: var(--sc-space-md);
      background: var(--sc-bg-elevated);
      border: 1px solid var(--sc-border-subtle);
      border-radius: var(--sc-radius);
    }
    .expand-panel-header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-bottom: var(--sc-space-md);
      font-weight: var(--sc-weight-semibold);
      font-size: var(--sc-text-base);
      color: var(--sc-text);
    }
    @media (prefers-reduced-motion: reduce) {
      * {
        animation-duration: 0s !important;
      }
    }
  `;

  @state() private tools: ToolDef[] = [];
  @state() private loading = true;
  @state() private error = "";
  @state() private _expandedTool: string | null = null;

  protected override async load(): Promise<void> {
    await this.loadTools();
  }

  private async loadTools(): Promise<void> {
    if (!this.gateway) {
      this.loading = false;
      return;
    }
    this.loading = true;
    try {
      const payload = await this.gateway.request<{ tools?: ToolDef[] }>("tools.catalog", {});
      this.tools = payload?.tools ?? [];
    } catch (e) {
      this.tools = [];
      this.error = friendlyError(e);
    } finally {
      this.loading = false;
    }
  }

  private get tableRows(): Record<string, unknown>[] {
    return this.tools.map((t) => ({
      name: t.name ?? "unnamed",
      description: t.description ?? "",
      paramsCount: countParams(t.parameters),
      parameters: resolveParams(t.parameters) ?? t.parameters,
    }));
  }

  private readonly columns: DataTableColumnV2[] = [
    { key: "name", label: "Name", sortable: true },
    { key: "description", label: "Description" },
    {
      key: "paramsCount",
      label: "Parameters",
      align: "right",
      sortable: true,
      render: (v) => (v != null && typeof v === "number" ? String(v) : "0"),
    },
  ];

  private _onRowClick(e: CustomEvent<{ row: Record<string, unknown>; index: number }>): void {
    const name = String(e.detail.row.name ?? "");
    this._expandedTool = this._expandedTool === name ? null : name;
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
  }

  private get _expandedToolData(): unknown {
    if (!this._expandedTool) return undefined;
    const t = this.tools.find((x) => (x.name ?? "unnamed") === this._expandedTool);
    return t?.parameters;
  }

  private _renderSkeleton() {
    return html`
      <sc-page-hero role="region" aria-label="Tools overview">
        <sc-section-header
          heading="Tools"
          description="Available tool integrations and their configurations"
        ></sc-section-header>
      </sc-page-hero>
      <sc-stats-row>
        <sc-skeleton variant="card" height="90px"></sc-skeleton>
        <sc-skeleton variant="card" height="90px"></sc-skeleton>
        <sc-skeleton variant="card" height="90px"></sc-skeleton>
      </sc-stats-row>
      <div class="table-section">
        <sc-skeleton variant="card" height="200px"></sc-skeleton>
      </div>
    `;
  }

  private _renderContent() {
    const rows = this.tableRows;
    const count = this.tools.length;
    const expandedParams = this._expandedToolData;

    return html`
      <sc-page-hero role="region" aria-label="Tools overview">
        <sc-section-header
          heading="Tools"
          description="Available tool integrations and their configurations"
        ></sc-section-header>
      </sc-page-hero>
      <sc-stats-row>
        <sc-stat-card
          .value=${count}
          label="Total Tools"
          style="--sc-stagger-delay: 0ms"
        ></sc-stat-card>
        <sc-stat-card
          .value=${this.tools.filter((t) => countParams(t.parameters) > 0).length}
          label="With Parameters"
          style="--sc-stagger-delay: 50ms"
        ></sc-stat-card>
        <sc-stat-card
          .value=${rows.reduce((sum, r) => sum + (Number(r.paramsCount) || 0), 0)}
          label="Total Parameters"
          style="--sc-stagger-delay: 100ms"
        ></sc-stat-card>
      </sc-stats-row>
      ${this.error
        ? html`<sc-empty-state
            .icon=${icons.warning}
            heading="Error"
            description=${this.error}
          ></sc-empty-state>`
        : nothing}
      <div class="table-section" role="region" aria-label="Tools table">
        ${rows.length === 0
          ? html`
              <sc-empty-state
                .icon=${icons.wrench}
                heading="No tools available"
                description="Tools will appear here when the gateway provides them."
              ></sc-empty-state>
            `
          : html`
              <sc-data-table-v2
                .columns=${this.columns}
                .rows=${rows}
                searchable
                @sc-row-click=${this._onRowClick}
              ></sc-data-table-v2>
              ${expandedParams !== undefined
                ? html`
                    <div class="expand-panel" role="region" aria-label="Parameter schema">
                      <div class="expand-panel-header">
                        <span>Parameters: ${this._expandedTool}</span>
                        <sc-button
                          variant="ghost"
                          size="sm"
                          @click=${() => {
                            this._expandedTool = null;
                          }}
                          aria-label="Close parameter view"
                        >
                          Close
                        </sc-button>
                      </div>
                      <sc-json-viewer
                        .data=${expandedParams}
                        root-label="parameters"
                      ></sc-json-viewer>
                    </div>
                  `
                : nothing}
            `}
      </div>
    `;
  }

  override render() {
    if (this.loading) {
      return this._renderSkeleton();
    }
    return this._renderContent();
  }
}
