import { LitElement, html, css, nothing } from "lit";
import { customElement, property, state } from "lit/decorators.js";

@customElement("sc-modal")
export class ScModal extends LitElement {
  static override styles = css`
    .backdrop {
      position: fixed;
      inset: 0;
      z-index: 9999;
      background: color-mix(in srgb, var(--sc-bg) 50%, transparent);
      backdrop-filter: blur(4px);
      -webkit-backdrop-filter: blur(4px);
      display: flex;
      align-items: center;
      justify-content: center;
      padding: var(--sc-space-lg);
      box-sizing: border-box;
      animation: sc-fade-in var(--sc-duration-normal) var(--sc-ease-out);
    }

    .backdrop.closing {
      animation: sc-fade-out var(--sc-duration-fast) var(--sc-ease-in) forwards;
    }

    .panel {
      max-width: 480px;
      width: 100%;
      background: var(--sc-bg-overlay);
      border-radius: var(--sc-radius-xl);
      box-shadow: var(--sc-shadow-lg);
      animation: sc-scale-in var(--sc-duration-normal) var(--sc-spring-out);
    }

    .panel.closing {
      animation: sc-scale-out var(--sc-duration-fast) var(--sc-ease-in) forwards;
    }

    .header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: var(--sc-space-md) var(--sc-space-lg);
      border-bottom: 1px solid var(--sc-border);
    }

    .heading {
      font-size: var(--sc-text-lg);
      font-weight: var(--sc-weight-semibold);
      color: var(--sc-text);
      margin: 0;
    }

    .close-btn {
      display: flex;
      align-items: center;
      justify-content: center;
      width: 24px;
      height: 24px;
      padding: 0;
      background: transparent;
      border: none;
      border-radius: var(--sc-radius-sm);
      color: var(--sc-text-muted);
      cursor: pointer;
      transition: color var(--sc-duration-fast);
    }

    .close-btn:hover {
      color: var(--sc-text);
    }

    .body {
      padding: var(--sc-space-lg);
    }
  `;

  @property({ type: Boolean }) open = false;
  @property({ type: String }) heading = "";
  @state() private _closing = false;
  private _closeTimeout: ReturnType<typeof setTimeout> | null = null;

  override updated(changedProperties: Map<string, unknown>): void {
    if (changedProperties.has("open")) {
      if (this.open) {
        this._closing = false;
        if (this._closeTimeout) {
          clearTimeout(this._closeTimeout);
          this._closeTimeout = null;
        }
        requestAnimationFrame(() => this._focusCloseButton());
      } else if (changedProperties.get("open") === true) {
        this._closing = true;
        this._closeTimeout = setTimeout(() => {
          this._closing = false;
          this._closeTimeout = null;
        }, 100);
      }
    }
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    if (this._closeTimeout) clearTimeout(this._closeTimeout);
  }

  private _focusCloseButton(): void {
    const btn = this.renderRoot.querySelector<HTMLButtonElement>(".close-btn");
    btn?.focus();
  }

  private _onBackdropClick(e: MouseEvent): void {
    if (e.target === e.currentTarget) {
      this._close();
    }
  }

  private _onKeyDown(e: KeyboardEvent): void {
    if (e.key === "Escape") {
      this._close();
    }
  }

  private _close(): void {
    this.dispatchEvent(new CustomEvent("close", { bubbles: true, composed: true }));
  }

  override render() {
    if (!this.open && !this._closing) return nothing;

    return html`
      <div
        class="backdrop ${this._closing ? "closing" : ""}"
        role="dialog"
        aria-modal="true"
        aria-labelledby=${this.heading ? "modal-heading" : undefined}
        @click=${this._onBackdropClick}
        @keydown=${this._onKeyDown}
      >
        <div
          class="panel ${this._closing ? "closing" : ""}"
          @click=${(e: MouseEvent) => e.stopPropagation()}
        >
          <header class="header">
            ${this.heading
              ? html`<h2 id="modal-heading" class="heading">${this.heading}</h2>`
              : nothing}
            <button class="close-btn" aria-label="Close" @click=${this._close}>×</button>
          </header>
          <div class="body">
            <slot></slot>
          </div>
        </div>
      </div>
    `;
  }
}
