import { LitElement, html, css } from "lit";
import { customElement, property, state } from "lit/decorators.js";
import { unsafeHTML } from "lit/directives/unsafe-html.js";
import DOMPurify from "dompurify";
import "./hu-button.js";

/**
 * Renders Live Canvas / A2UI markup (HTML or SVG) in a sandboxed shadow subtree.
 * Scripts are stripped; use with agent-driven visual content only.
 */
@customElement("hu-canvas")
export class HuCanvas extends LitElement {
  @property({ type: String }) title = "";
  @property({ type: String }) content = "";
  /** html | svg | mockup */
  @property({ type: String }) format: "html" | "svg" | "mockup" = "html";

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

    .stage-wrap {
      flex: 1;
      min-height: 12rem;
      position: relative;
      overflow: auto;
      padding: var(--hu-space-md);
      background: var(--hu-bg-elevated);
    }

    .mockup {
      max-width: 24rem;
      margin: 0 auto;
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
      padding: var(--hu-space-sm);
    }

    .sandbox {
      width: 100%;
      min-height: 8rem;
    }

    .sandbox :where(img, svg) {
      max-width: 100%;
      height: auto;
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
    }

    @media (prefers-reduced-motion: reduce) {
      * {
        transition-duration: 0s !important;
      }
    }
  `;

  private _sanitize(htmlIn: string): string {
    const isSvg = this.format === "svg";
    return DOMPurify.sanitize(htmlIn, {
      USE_PROFILES: isSvg ? { svg: true, svgFilters: true } : { html: true },
      FORBID_TAGS: ["script", "iframe", "object", "embed", "link"],
      FORBID_ATTR: ["onerror", "onclick", "onload", "onmouseover"],
    });
  }

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
    const safe = hasContent ? this._sanitize(this.content) : "";
    const inner = hasContent
      ? html`<div class="sandbox">${unsafeHTML(safe)}</div>`
      : html`<div class="empty" role="status">No canvas content yet.</div>`;

    const framed =
      this.format === "mockup"
        ? html`
            <div class="mockup" aria-label="Device mockup frame">
              <div class="mockup-notch" aria-hidden="true"></div>
              <div class="mockup-body">${inner}</div>
            </div>
          `
        : inner;

    return html`
      <div class="toolbar">
        <h2>${this.title || "Canvas"}</h2>
        <hu-button
          variant="tonal"
          size="sm"
          @click=${() => this._toggleFullscreen()}
          aria-label=${this._fullscreen ? "Exit fullscreen" : "Enter fullscreen"}
        >
          ${this._fullscreen ? "Exit" : "Fullscreen"}
        </hu-button>
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
