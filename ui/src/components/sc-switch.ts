import { LitElement, html, css } from "lit";
import { customElement, property } from "lit/decorators.js";

@customElement("sc-switch")
export class ScSwitch extends LitElement {
  @property({ type: Boolean }) checked = false;
  @property({ type: Boolean }) disabled = false;
  @property({ type: String }) label = "";

  static override styles = css`
    :host {
      display: inline-flex;
      align-items: center;
      gap: var(--sc-space-sm);
    }

    .switch {
      position: relative;
      flex-shrink: 0;
      width: 2.25rem;
      height: 1.25rem;
      background: var(--sc-border);
      border-radius: var(--sc-radius-full);
      cursor: pointer;
      transition: background-color var(--sc-duration-fast) var(--sc-ease-out);
    }

    .switch.checked {
      background: var(--sc-accent);
    }

    .switch:focus-within {
      outline: var(--sc-focus-ring-width) solid var(--sc-focus-ring);
      outline-offset: var(--sc-focus-ring-offset);
    }

    .switch:disabled {
      opacity: var(--sc-opacity-disabled);
      cursor: not-allowed;
    }

    @media (prefers-reduced-motion: reduce) {
      .switch {
        transition: none;
      }
    }

    .thumb {
      position: absolute;
      top: var(--sc-space-2xs);
      left: var(--sc-space-2xs);
      width: 1rem;
      height: 1rem;
      background: var(--sc-bg-overlay);
      border-radius: var(--sc-radius-full);
      box-shadow: var(--sc-shadow-sm);
      transition: transform var(--sc-duration-fast) var(--sc-spring-out);
    }

    .switch.checked .thumb {
      transform: translateX(1rem);
    }

    @media (prefers-reduced-motion: reduce) {
      .thumb {
        transition: none;
      }
    }

    .label {
      font-size: var(--sc-text-sm);
      color: var(--sc-text);
      font-family: var(--sc-font);
      font-weight: var(--sc-weight-medium);
    }

    input {
      position: absolute;
      width: 1px;
      height: 1px;
      padding: 0;
      margin: -1px;
      overflow: hidden;
      clip: rect(0, 0, 0, 0);
      white-space: nowrap;
      border: 0;
    }
  `;

  private _onClick(): void {
    if (this.disabled) return;
    this.checked = !this.checked;
    this.dispatchEvent(
      new CustomEvent("sc-change", {
        bubbles: true,
        composed: true,
        detail: { checked: this.checked },
      }),
    );
  }

  private _onKeyDown(e: KeyboardEvent): void {
    if (e.key === " " || e.key === "Enter") {
      e.preventDefault();
      this._onClick();
    }
  }

  override render() {
    return html`
      <div
        class="switch ${this.checked ? "checked" : ""}"
        role="switch"
        aria-checked=${this.checked}
        aria-label=${this.label || "Toggle"}
        aria-disabled=${this.disabled}
        tabindex=${this.disabled ? -1 : 0}
        @click=${this._onClick}
        @keydown=${this._onKeyDown}
      >
        <span class="thumb" aria-hidden="true"></span>
      </div>
      ${this.label ? html`<span class="label">${this.label}</span>` : null}
    `;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "sc-switch": ScSwitch;
  }
}
