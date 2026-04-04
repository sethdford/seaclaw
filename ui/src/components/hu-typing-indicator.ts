import { LitElement, html, css, nothing } from "lit";
import { customElement, property } from "lit/decorators.js";

@customElement("hu-typing-indicator")
export class ScTypingIndicator extends LitElement {
  @property({ type: String }) elapsed = "";

  static override styles = css`
    @keyframes hu-indicator-enter {
      from {
        opacity: 0;
        transform: translateY(var(--hu-space-sm));
      }
      to {
        opacity: 1;
        transform: translateY(0);
      }
    }

    @keyframes hu-dot-wave {
      0%,
      100% {
        opacity: 0.4;
        transform: scale(0.6);
      }
      50% {
        opacity: 1;
        transform: scale(1.2);
      }
    }

    @keyframes hu-thinking-shimmer {
      from {
        background-position: -200% center;
      }
      to {
        background-position: 200% center;
      }
    }

    :host {
      display: block;
    }

    .indicator {
      display: inline-flex;
      align-items: center;
      gap: var(--hu-space-sm);
      padding: var(--hu-space-xs) var(--hu-space-md);
      background: linear-gradient(
        90deg,
        var(--hu-surface-container) 0%,
        color-mix(in srgb, var(--hu-accent) 8%, var(--hu-surface-container)) 50%,
        var(--hu-surface-container) 100%
      );
      background-size: 200% 100%;
      border: 1px solid var(--hu-border-subtle);
      border-radius: var(--hu-radius-full);
      animation:
        hu-indicator-enter var(--hu-duration-normal) var(--hu-ease-out) both,
        hu-thinking-shimmer calc(var(--hu-duration-slowest) * 3) var(--hu-ease-in-out) infinite;
    }

    .label {
      font-family: var(--hu-font);
      font-size: var(--hu-text-xs);
      font-weight: var(--hu-weight-medium);
      color: var(--hu-accent);
      white-space: nowrap;
    }

    .dots {
      display: flex;
      align-items: center;
      gap: var(--hu-space-xs);
    }

    .dot {
      width: var(--hu-space-xs);
      height: var(--hu-space-xs);
      border-radius: var(--hu-radius-full);
      background: var(--hu-accent);
      animation: hu-dot-wave var(--hu-duration-slow) var(--hu-ease-in-out) infinite;
    }

    .dot:nth-child(2) {
      animation-delay: var(--hu-duration-normal);
    }

    .dot:nth-child(3) {
      animation-delay: var(--hu-duration-moderate);
    }

    .elapsed {
      font-family: var(--hu-font);
      font-size: var(--hu-text-xs);
      color: var(--hu-text-muted);
      white-space: nowrap;
      font-variant-numeric: tabular-nums;
      opacity: 0;
      animation: hu-indicator-enter var(--hu-duration-normal) var(--hu-ease-out) 3s both;
    }

    @media (prefers-reduced-motion: reduce) {
      .indicator {
        animation: none;
        background: var(--hu-surface-container);
      }

      .dot {
        animation: none;
        opacity: 0.6;
      }

      .elapsed {
        animation: none;
        opacity: 1;
      }
    }
  `;

  override render() {
    return html`
      <div class="indicator" role="status" aria-live="polite" aria-label="Assistant is thinking">
        <span class="label">Thinking</span>
        <span class="dots" aria-hidden="true">
          <span class="dot"></span>
          <span class="dot"></span>
          <span class="dot"></span>
        </span>
        ${this.elapsed ? html`<span class="elapsed">${this.elapsed}</span>` : nothing}
      </div>
    `;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "hu-typing-indicator": ScTypingIndicator;
  }
}
