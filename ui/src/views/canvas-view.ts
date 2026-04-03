import { html, css, nothing } from "lit";
import { customElement, state } from "lit/decorators.js";
import { scrollEntranceStyles } from "../styles/scroll-entrance.js";
import { GatewayAwareLitElement } from "../gateway-aware.js";
import { GatewayClient } from "../gateway.js";
import { icons } from "../icons.js";
import "../components/hu-button.js";
import "../components/hu-card.js";
import "../components/hu-empty-state.js";
import "../components/hu-canvas.js";

interface CanvasEntry {
  id: string;
  title: string;
  format: "html" | "svg" | "mockup";
  content: string;
  status: "active" | "closed";
}

@customElement("canvas-view")
export class CanvasView extends GatewayAwareLitElement {
  @state() private canvases: CanvasEntry[] = [];
  @state() private selectedId: string | null = null;

  private _onGateway = ((e: Event) => {
    const ev = e as CustomEvent<{ event: string; payload: Record<string, unknown> }>;
    if (ev.detail?.event !== "canvas") return;
    const p = ev.detail.payload ?? {};
    const action = String(p.action ?? "");
    const canvasId = typeof p.canvas_id === "string" ? p.canvas_id : "";
    if (!canvasId) return;

    if (action === "create") {
      const title = typeof p.title === "string" ? p.title : canvasId;
      const format =
        p.format === "svg" || p.format === "mockup" || p.format === "html" ? p.format : "html";
      const next = this.canvases.filter((c) => c.id !== canvasId);
      next.push({ id: canvasId, title, format, content: "", status: "active" });
      this.canvases = next;
      this.selectedId = canvasId;
      return;
    }

    if (action === "update") {
      const content = typeof p.content === "string" ? p.content : "";
      const format =
        p.format === "svg" || p.format === "mockup" || p.format === "html" ? p.format : undefined;
      this.canvases = this.canvases.map((c) => {
        if (c.id !== canvasId) return c;
        return {
          ...c,
          content,
          ...(format ? { format } : {}),
        };
      });
      if (!this.selectedId) this.selectedId = canvasId;
      return;
    }

    if (action === "clear") {
      this.canvases = this.canvases.map((c) =>
        c.id === canvasId ? { ...c, content: "" } : c,
      );
      return;
    }

    if (action === "close") {
      this.canvases = this.canvases.filter((c) => c.id !== canvasId);
      if (this.selectedId === canvasId) {
        this.selectedId = this.canvases[0]?.id ?? null;
      }
    }
  }) as EventListener;

  static override styles = [
    scrollEntranceStyles,
    css`
      :host {
        view-transition-name: view-canvas;
        display: block;
        color: var(--hu-text);
        max-width: 75rem;
        contain: layout style;
        container-type: inline-size;
        padding: var(--hu-space-lg) var(--hu-space-xl);
      }

      .layout {
        display: block;
      }

      @media (min-width: 1240px) /* --hu-breakpoint-wide */ {
        .layout.has-detail {
          display: grid;
          grid-template-columns: minmax(14rem, 1fr) minmax(0, 2fr);
          gap: var(--hu-space-lg);
          align-items: start;
        }
      }

      .list-col h1 {
        font-size: var(--hu-text-xl);
        margin: 0 0 var(--hu-space-md);
      }

      .list {
        display: flex;
        flex-direction: column;
        gap: var(--hu-space-sm);
      }

      .row {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: var(--hu-space-sm);
        padding: var(--hu-space-sm) var(--hu-space-md);
        border-radius: var(--hu-radius-md);
        border: 1px solid var(--hu-border-subtle);
        background: var(--hu-surface-container);
        cursor: pointer;
        text-align: left;
        width: 100%;
        font-family: var(--hu-font);
        color: var(--hu-text);
        transition: background var(--hu-duration-fast) var(--hu-ease-out),
          border-color var(--hu-duration-fast) var(--hu-ease-out);
      }

      .row:hover {
        background: var(--hu-surface-container-high);
      }

      .row.selected {
        border-color: var(--hu-accent);
        box-shadow: 0 0 0 1px color-mix(in srgb, var(--hu-accent) 35%, transparent);
      }

      .row-title {
        font-size: var(--hu-text-sm);
        font-weight: var(--hu-weight-semibold);
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
      }

      .row-meta {
        font-size: var(--hu-text-xs);
        color: var(--hu-text-secondary);
      }

      .detail {
        border-radius: var(--hu-radius-xl);
        border: 1px solid var(--hu-border-subtle);
        background: var(--hu-surface-container);
        box-shadow: var(--hu-shadow-card);
        min-height: 20rem;
        display: flex;
        flex-direction: column;
        overflow: hidden;
      }

      .detail-header {
        padding: var(--hu-space-md);
        border-bottom: 1px solid var(--hu-border-subtle);
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: var(--hu-space-md);
      }

      .detail-header h2 {
        margin: 0;
        font-size: var(--hu-text-lg);
      }

      .detail-body {
        flex: 1;
        min-height: 0;
        display: flex;
        flex-direction: column;
      }

      hu-canvas {
        flex: 1;
        min-height: 0;
      }

      @media (prefers-reduced-motion: reduce) {
        * {
          transition-duration: 0s !important;
        }
      }
    `,
  ];

  override connectedCallback(): void {
    super.connectedCallback();
    this.gateway?.addEventListener(GatewayClient.EVENT_GATEWAY, this._onGateway);
  }

  override disconnectedCallback(): void {
    this.gateway?.removeEventListener(GatewayClient.EVENT_GATEWAY, this._onGateway);
    super.disconnectedCallback();
  }

  protected override onGatewaySwapped(
    previous: GatewayClient | null,
    current: GatewayClient,
  ): void {
    previous?.removeEventListener(GatewayClient.EVENT_GATEWAY, this._onGateway);
    current.addEventListener(GatewayClient.EVENT_GATEWAY, this._onGateway);
  }

  protected override async load(): Promise<void> {
    this.lastLoadedAt = Date.now();
  }

  override render() {
    const selected = this.canvases.find((c) => c.id === this.selectedId) ?? null;
    const hasList = this.canvases.length > 0;

    return html`
      <div class="layout ${selected ? "has-detail" : ""}">
        <div class="list-col">
          <h1>Live Canvas</h1>
          ${!hasList
            ? html`
                <hu-empty-state
                  .icon=${icons.image}
                  heading="No active canvases"
                  description="When the agent uses the canvas tool, visual surfaces appear here."
                ></hu-empty-state>
              `
            : html`
                <div class="list" role="list">
                  ${this.canvases.map(
                    (c) => html`
                      <button
                        type="button"
                        class="row ${this.selectedId === c.id ? "selected" : ""}"
                        role="listitem"
                        @click=${() => {
                          this.selectedId = c.id;
                        }}
                      >
                        <div>
                          <div class="row-title">${c.title || c.id}</div>
                          <div class="row-meta">${c.format} · ${c.content ? "has content" : "empty"}</div>
                        </div>
                      </button>
                    `,
                  )}
                </div>
              `}
        </div>
        ${selected
          ? html`
              <div class="detail">
                <div class="detail-header">
                  <h2>${selected.title || selected.id}</h2>
                </div>
                <div class="detail-body">
                  <hu-canvas
                    .title=${selected.title || selected.id}
                    .content=${selected.content}
                    .format=${selected.format}
                  ></hu-canvas>
                </div>
              </div>
            `
            : nothing}
      </div>
    `;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "canvas-view": CanvasView;
  }
}
