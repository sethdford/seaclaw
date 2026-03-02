import { LitElement, html, css, nothing } from "lit";
import { customElement, state } from "lit/decorators.js";
import type { GatewayClient } from "../gateway.js";

interface ProviderItem {
  name?: string;
  has_key?: boolean;
  base_url?: string;
  native_tools?: boolean;
  is_default?: boolean;
}

interface ModelsListRes {
  default_model?: string;
  providers?: ProviderItem[];
}

interface ConfigGetRes {
  default_model?: string;
  default_provider?: string;
}

function unwrapPayload(res: unknown): unknown {
  const r = res as { payload?: unknown; result?: unknown };
  return r?.payload ?? r?.result ?? res;
}

@customElement("sc-models-view")
export class ScModelsView extends LitElement {
  static override styles = css`
    :host {
      display: block;
    }
    h2 {
      margin: 0 0 1rem;
      font-size: 1.25rem;
      font-weight: 600;
      color: var(--sc-text);
    }
    .info-bar {
      display: flex;
      flex-wrap: wrap;
      gap: 1rem;
      padding: 0.75rem 1rem;
      background: var(--sc-bg-surface);
      border: 1px solid var(--sc-border);
      border-radius: var(--sc-radius);
      margin-bottom: 1.5rem;
      font-size: 0.875rem;
    }
    .info-item {
      color: var(--sc-text-muted);
    }
    .info-item strong {
      color: var(--sc-text);
      margin-right: 0.25rem;
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
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
      flex-wrap: wrap;
    }
    .card-name {
      font-weight: 600;
      font-size: 1rem;
      color: var(--sc-text);
    }
    .card-name.default {
      color: var(--sc-accent);
    }
    .badge {
      font-size: 0.625rem;
      padding: 0.2rem 0.4rem;
      border-radius: 4px;
      background: var(--sc-bg-elevated);
      color: var(--sc-text-muted);
    }
    .badge.default {
      background: var(--sc-accent);
      color: var(--sc-bg);
    }
    .card-url {
      font-size: 0.75rem;
      font-family: var(--sc-font-mono);
      color: var(--sc-text-muted);
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
      margin-top: 0.25rem;
    }
    .key-status {
      font-size: 0.8125rem;
      margin-top: 0.5rem;
      display: flex;
      align-items: center;
      gap: 0.35rem;
    }
    .key-status.has {
      color: #22c55e;
    }
    .key-status.missing {
      color: #ef4444;
    }
    .loading {
      color: var(--sc-text-muted);
      font-size: 0.875rem;
    }
  `;

  @state() private defaultModel = "";
  @state() private defaultProvider = "";
  @state() private providers: ProviderItem[] = [];
  @state() private loading = true;

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
      return;
    }
    this.loading = true;
    try {
      const [modelsRes, configRes] = await Promise.all([
        gw.request<ModelsListRes>("models.list", {}).catch(() => ({})),
        gw.request<ConfigGetRes>("config.get", {}).catch(() => ({})),
      ]);
      const modelsPayload = unwrapPayload(modelsRes) as ModelsListRes;
      const configPayload = unwrapPayload(configRes) as ConfigGetRes;
      this.defaultModel =
        modelsPayload?.default_model ?? configPayload?.default_model ?? "";
      this.defaultProvider = configPayload?.default_provider ?? "";
      this.providers = modelsPayload?.providers ?? [];
    } catch {
      this.providers = [];
      this.defaultModel = "";
      this.defaultProvider = "";
    } finally {
      this.loading = false;
    }
  }

  private truncateUrl(url?: string): string {
    if (!url) return "—";
    if (url.length <= 50) return url;
    return url.slice(0, 24) + "…" + url.slice(-24);
  }

  override render() {
    if (this.loading) {
      return html`
        <h2>Models & Providers</h2>
        <p class="loading">Loading...</p>
      `;
    }

    return html`
      <h2>Models & Providers</h2>
      <div class="info-bar">
        <span class="info-item"
          ><strong>Default provider:</strong> ${this.defaultProvider ||
          "—"}</span
        >
        <span class="info-item"
          ><strong>Default model:</strong> ${this.defaultModel || "—"}</span
        >
      </div>
      <div class="grid">
        ${this.providers.length === 0
          ? html`<p class="loading">No providers configured</p>`
          : this.providers.map(
              (p) => html`
                <div class="card">
                  <div class="card-header">
                    <span class="card-name ${p.is_default ? "default" : ""}"
                      >${p.name ?? "unnamed"}</span
                    >
                    ${p.is_default
                      ? html`<span class="badge default">default</span>`
                      : nothing}
                    ${p.native_tools
                      ? html`<span class="badge">native tools</span>`
                      : nothing}
                  </div>
                  <div class="key-status ${p.has_key ? "has" : "missing"}">
                    ${p.has_key ? "✓ API key" : "✗ No API key"}
                  </div>
                  <div class="card-url" title=${p.base_url ?? ""}>
                    ${this.truncateUrl(p.base_url)}
                  </div>
                </div>
              `,
            )}
      </div>
    `;
  }
}
