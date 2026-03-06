import { LitElement, html, css } from "lit";
import { customElement, property } from "lit/decorators.js";
import { icons } from "../icons.js";

@customElement("sc-message-branch")
export class ScMessageBranch extends LitElement {
  @property({ type: Number }) branches = 1;
  @property({ type: Number }) current = 0;

  static override styles = css`
    :host {
      display: inline-block;
    }

    .pill {
      display: inline-flex;
      align-items: center;
      gap: var(--sc-space-xs);
      padding: var(--sc-space-xs) var(--sc-space-sm);
      border-radius: var(--sc-radius-full);
      background: color-mix(
        in srgb,
        var(--sc-surface) var(--sc-glass-subtle-bg-opacity, 4%),
        transparent
      );
      backdrop-filter: blur(var(--sc-glass-subtle-blur, 12px))
        saturate(var(--sc-glass-subtle-saturate, 120%));
      -webkit-backdrop-filter: blur(var(--sc-glass-subtle-blur, 12px))
        saturate(var(--sc-glass-subtle-saturate, 120%));
      border: 1px solid
        color-mix(in srgb, var(--sc-border) var(--sc-glass-subtle-border-opacity, 5%), transparent);
      font-family: var(--sc-font);
      font-size: var(--sc-text-sm);
      color: var(--sc-text-muted);
    }

    .btn {
      display: flex;
      align-items: center;
      justify-content: center;
      padding: var(--sc-space-2xs);
      border: none;
      background: transparent;
      color: var(--sc-text-muted);
      cursor: pointer;
      border-radius: var(--sc-radius);
      transition: color var(--sc-duration-fast) var(--sc-ease-out);
    }

    .btn:hover:not(:disabled) {
      color: var(--sc-text);
    }

    .btn:disabled {
      opacity: 0.4;
      cursor: not-allowed;
    }

    .btn:focus-visible {
      outline: var(--sc-focus-ring-width) solid var(--sc-focus-ring);
      outline-offset: 2px;
    }

    .btn svg {
      width: 14px;
      height: 14px;
    }

    .label {
      min-width: 2.5em;
      text-align: center;
    }
  `;

  private _prev() {
    if (this.current > 0) {
      this.current--;
      this._dispatchChange();
    }
  }

  private _next() {
    if (this.current < this.branches - 1) {
      this.current++;
      this._dispatchChange();
    }
  }

  private _dispatchChange() {
    this.dispatchEvent(
      new CustomEvent("branch-change", {
        detail: { branch: this.current },
        bubbles: true,
        composed: true,
      }),
    );
  }

  private _onKeyDown(e: KeyboardEvent) {
    if (e.key === "ArrowLeft") {
      e.preventDefault();
      this._prev();
    } else if (e.key === "ArrowRight") {
      e.preventDefault();
      this._next();
    }
  }

  override render() {
    const canPrev = this.current > 0;
    const canNext = this.current < this.branches - 1;
    const displayCurrent = Math.min(this.current + 1, this.branches);

    return html`
      <div
        class="pill"
        role="group"
        tabindex="0"
        aria-label="Branch ${displayCurrent} of ${this.branches}"
        @keydown=${this._onKeyDown}
      >
        <button
          class="btn"
          type="button"
          ?disabled=${!canPrev}
          aria-label="Previous branch"
          @click=${this._prev}
        >
          ${icons.chevron}
        </button>
        <span class="label">${displayCurrent} / ${this.branches}</span>
        <button
          class="btn"
          type="button"
          ?disabled=${!canNext}
          aria-label="Next branch"
          @click=${this._next}
        >
          ${icons["chevron-right"]}
        </button>
      </div>
    `;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "sc-message-branch": ScMessageBranch;
  }
}
