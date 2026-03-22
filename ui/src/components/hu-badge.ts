import { LitElement, html, css } from "lit";
import { customElement, property } from "lit/decorators.js";

type BadgeVariant = "success" | "warning" | "error" | "info" | "neutral";

@customElement("hu-badge")
export class ScBadge extends LitElement {
  @property({ type: String }) variant: BadgeVariant = "neutral";
  @property({ type: Boolean }) dot = false;

  static override styles = css`
    :host {
      display: inline-flex;
    }

    .badge {
      display: inline-flex;
      align-items: center;
      gap: var(--hu-space-xs);
      font-size: var(--hu-text-xs);
      font-weight: var(--hu-weight-medium);
      padding: var(--hu-space-2xs) var(--hu-space-sm);
      border-radius: var(--hu-radius-full);
      font-family: var(--hu-font);
    }

    /* Variants */
    .badge.variant-neutral {
      background: color-mix(in srgb, var(--hu-dynamic-neutral-700) 14%, var(--hu-bg-elevated));
      color: var(--hu-text-muted);
    }

    .badge.variant-success {
      background: var(--hu-success-dim);
      color: var(--hu-success);
    }

    .badge.variant-warning {
      background: var(--hu-warning-dim);
      color: var(--hu-warning);
    }

    .badge.variant-error {
      background: var(--hu-error-dim);
      color: var(--hu-error);
    }

    .badge.variant-info {
      background: color-mix(in srgb, var(--hu-dynamic-tertiary-500) 16%, transparent);
      color: var(--hu-accent-tertiary);
    }

    .dot {
      width: var(--hu-space-xs);
      height: var(--hu-space-xs);
      border-radius: 50%;
      flex-shrink: 0;
    }

    .variant-neutral .dot {
      background: var(--hu-text-muted);
    }
    .variant-success .dot {
      background: var(--hu-success);
    }
    .variant-warning .dot {
      background: var(--hu-warning);
    }
    .variant-error .dot {
      background: var(--hu-error);
    }
    .variant-info .dot {
      background: var(--hu-dynamic-tertiary-600);
    }
  `;

  override render() {
    const classes = ["badge", `variant-${this.variant}`].join(" ");

    return html`
      <span class=${classes}>
        ${this.dot ? html`<span class="dot" aria-hidden="true"></span>` : null}
        <slot></slot>
      </span>
    `;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "hu-badge": ScBadge;
  }
}
