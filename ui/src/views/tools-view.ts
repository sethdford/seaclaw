import { LitElement, html, css } from "lit";
import { customElement, state } from "lit/decorators.js";
import type { GatewayClient } from "../gateway.js";

interface ToolDef {
  name?: string;
  description?: string;
  parameters?: unknown;
}

function unwrapPayload(res: unknown): unknown {
  const r = res as { payload?: unknown };
  return r?.payload ?? res;
}

@customElement("sc-tools-view")
export class ScToolsView extends LitElement {
  static override styles = css`
    :host {
      display: block;
    }
    .search {
      margin-bottom: 1rem;
      max-width: 400px;
    }
    .search input {
      width: 100%;
      padding: 0.5rem 1rem;
      background: var(--sc-bg-surface);
      border: 1px solid var(--sc-border);
      border-radius: var(--sc-radius);
      color: var(--sc-text);
      font-size: 0.875rem;
    }
    .search input:focus {
      outline: none;
      border-color: var(--sc-accent);
    }
    .search input::placeholder {
      color: var(--sc-text-muted);
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
    .card-name {
      font-weight: 600;
      font-size: 1rem;
      color: var(--sc-accent);
      margin-bottom: 0.5rem;
    }
    .card-desc {
      font-size: 0.875rem;
      color: var(--sc-text-muted);
      margin-bottom: 0.75rem;
      line-height: 1.4;
    }
    .card-schema {
      font-size: 0.75rem;
      font-family: var(--sc-font-mono);
      color: var(--sc-text-muted);
      background: var(--sc-bg);
      padding: 0.5rem;
      border-radius: 4px;
      max-height: 6rem;
      overflow: auto;
    }
    .schema-toggle {
      background: none;
      border: none;
      color: var(--sc-accent);
      cursor: pointer;
      font-size: 0.75rem;
      padding: 0.25rem 0;
    }
  `;

  @state() private tools: ToolDef[] = [];
  @state() private filter = "";
  @state() private expandedCards = new Set<string>();

  private gateway: GatewayClient | null = null;

  override connectedCallback(): void {
    super.connectedCallback();
    this.gateway =
      (document.querySelector("sc-app") as { gateway?: GatewayClient })
        ?.gateway ?? null;
    this.loadTools();
  }

  private async loadTools(): Promise<void> {
    if (!this.gateway) return;
    try {
      const res = await this.gateway.request("tools.catalog", {});
      const payload = unwrapPayload(res) as { tools?: ToolDef[] };
      this.tools = payload?.tools ?? [];
    } catch {
      this.tools = [];
    }
  }

  private toggleSchema(name: string): void {
    const next = new Set(this.expandedCards);
    if (next.has(name)) next.delete(name);
    else next.add(name);
    this.expandedCards = next;
  }

  private get filteredTools(): ToolDef[] {
    const q = this.filter.toLowerCase().trim();
    if (!q) return this.tools;
    return this.tools.filter(
      (t) =>
        (t.name ?? "").toLowerCase().includes(q) ||
        (t.description ?? "").toLowerCase().includes(q),
    );
  }

  override render() {
    const filtered = this.filteredTools;

    return html`
      <div class="search">
        <input
          type="text"
          placeholder="Search tools..."
          .value=${this.filter}
          @input=${(e: Event) => {
            this.filter = (e.target as HTMLInputElement).value;
          }}
        />
      </div>
      <div class="grid">
        ${filtered.map(
          (t) => html`
            <div class="card">
              <div class="card-name">${t.name ?? "unnamed"}</div>
              <div class="card-desc">${t.description ?? ""}</div>
              ${t.parameters != null
                ? html`
                    <button
                      class="schema-toggle"
                      @click=${() => this.toggleSchema(t.name ?? "")}
                    >
                      ${this.expandedCards.has(t.name ?? "")
                        ? "Hide params"
                        : "Show params"}
                    </button>
                    ${this.expandedCards.has(t.name ?? "")
                      ? html`
                          <pre class="card-schema">
${JSON.stringify(t.parameters, null, 2)}</pre
                          >
                        `
                      : ""}
                  `
                : ""}
            </div>
          `,
        )}
      </div>
    `;
  }
}
