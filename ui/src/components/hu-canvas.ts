import { LitElement, html, css } from "lit";
import { customElement, property, state } from "lit/decorators.js";
import "./hu-button.js";
import "./hu-canvas-sandbox.js";

export type CanvasFormat = "html" | "svg" | "mockup" | "react" | "mermaid" | "markdown" | "code";

/**
 * Renders Live Canvas / A2UI content in a sandboxed iframe.
 * Supports HTML, SVG, React/JSX, Mermaid, Markdown, and code display.
 */
@customElement("hu-canvas")
export class HuCanvas extends LitElement {
  @property({ type: String }) title = "";
  @property({ type: String }) content = "";
  @property({ type: String }) format: CanvasFormat = "html";
  @property({ type: Object }) imports: Record<string, string> = {};
  @property({ type: String }) language = "";

  @state() private _fullscreen = false;

  static override styles = css`
    :host {
      display: flex;
      flex-direction: column;
      min-height: 0;
      flex: 1;
      color: var(--hu-text);
      font-family: var(--hu-font);
    }

    .toolbar {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: var(--hu-space-sm);
      padding: var(--hu-space-sm) var(--hu-space-md);
      border-bottom: 1px solid var(--hu-border-subtle);
      background: var(--hu-surface-container);
      flex-shrink: 0;
    }

    .toolbar h2 {
      margin: 0;
      font-size: var(--hu-text-sm);
      font-weight: var(--hu-weight-semibold);
      color: var(--hu-text-secondary);
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }

    .toolbar-actions {
      display: flex;
      align-items: center;
      gap: var(--hu-space-xs);
    }

    .format-badge {
      display: inline-flex;
      padding: 2px var(--hu-space-xs);
      border-radius: var(--hu-radius-sm);
      background: color-mix(in srgb, var(--hu-accent-tertiary) 15%, transparent);
      color: var(--hu-accent-tertiary);
      font-size: var(--hu-text-xs);
      font-weight: var(--hu-weight-semibold);
      text-transform: uppercase;
      letter-spacing: 0.05em;
    }

    .stage-wrap {
      flex: 1;
      min-height: 12rem;
      position: relative;
      overflow: auto;
      background: var(--hu-bg-elevated);
    }

    .mockup {
      max-width: 24rem;
      margin: var(--hu-space-md) auto;
      border-radius: var(--hu-radius-xl);
      border: var(--hu-space-xs) solid var(--hu-border-strong);
      box-shadow: var(--hu-shadow-lg);
      background: var(--hu-bg-surface);
      overflow: hidden;
      aspect-ratio: 9 / 19;
      display: flex;
      flex-direction: column;
    }

    .mockup-notch {
      height: var(--hu-space-md);
      background: var(--hu-surface-container-high);
      border-radius: 0 0 var(--hu-radius-md) var(--hu-radius-md);
      margin: 0 auto var(--hu-space-sm);
      width: 40%;
    }

    .mockup-body {
      flex: 1;
      min-height: 0;
      overflow: auto;
    }

    hu-canvas-sandbox {
      width: 100%;
    }

    .empty {
      display: flex;
      align-items: center;
      justify-content: center;
      min-height: 10rem;
      padding: var(--hu-space-xl);
      text-align: center;
      font-size: var(--hu-text-sm);
      color: var(--hu-text-secondary);
      border: 1px dashed var(--hu-border-subtle);
      border-radius: var(--hu-radius-lg);
      background: color-mix(in srgb, var(--hu-surface-container) 40%, transparent);
      margin: var(--hu-space-md);
    }

    @media (prefers-reduced-motion: reduce) {
      * {
        transition-duration: 0s !important;
      }
    }
  `;

  private _toggleFullscreen(): void {
    const root = this.renderRoot.querySelector(".stage-wrap") as HTMLElement | null;
    if (!root) return;
    if (!document.fullscreenElement) {
      void root.requestFullscreen().then(() => {
        this._fullscreen = true;
      });
    } else {
      void document.exitFullscreen().then(() => {
        this._fullscreen = false;
      });
    }
  }

  override connectedCallback(): void {
    super.connectedCallback();
    document.addEventListener("fullscreenchange", this._onFsChange);
  }

  override disconnectedCallback(): void {
    document.removeEventListener("fullscreenchange", this._onFsChange);
    super.disconnectedCallback();
  }

  private _onFsChange = (): void => {
    this._fullscreen = !!document.fullscreenElement;
    this.requestUpdate();
  };

  override render() {
    const hasContent = this.content.trim().length > 0;

    const sandbox = hasContent
      ? html`<hu-canvas-sandbox
          .content=${this.content}
          .format=${this.format}
          .imports=${this.imports}
          .language=${this.language}
        ></hu-canvas-sandbox>`
      : html`<div class="empty" role="status">No canvas content yet.</div>`;

    const framed =
      this.format === "mockup"
        ? html`
            <div class="mockup" aria-label="Device mockup frame">
              <div class="mockup-notch" aria-hidden="true"></div>
              <div class="mockup-body">${sandbox}</div>
            </div>
          `
        : sandbox;

    return html`
      <div class="toolbar">
        <h2>${this.title || "Canvas"}</h2>
        <div class="toolbar-actions">
          <span class="format-badge">${this.format}</span>
          <hu-button
            variant="tonal"
            size="sm"
            @click=${() => this._toggleFullscreen()}
            aria-label=${this._fullscreen ? "Exit fullscreen" : "Enter fullscreen"}
          >
            ${this._fullscreen ? "Exit" : "Fullscreen"}
          </hu-button>
        </div>
      </div>
      <div class="stage-wrap">${framed}</div>
    `;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "hu-canvas": HuCanvas;
  }
}
