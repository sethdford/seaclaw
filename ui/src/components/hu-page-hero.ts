import { LitElement, html, css } from "lit";
import { customElement } from "lit/decorators.js";

@customElement("hu-page-hero")
export class ScPageHero extends LitElement {
  static override styles = css`
    @keyframes hu-hero-shimmer {
      0%,
      100% {
        opacity: 0.5;
        transform: translateX(-10%) scale(1);
      }
      50% {
        opacity: 1;
        transform: translateX(10%) scale(1.05);
      }
    }

    :host {
      display: block;
      view-transition-name: hu-page-hero;
    }
    .hero {
      position: relative;
      padding: var(--hu-space-lg) var(--hu-space-lg) var(--hu-space-md);
      animation: hu-fade-in var(--hu-duration-normal) var(--hu-ease-out) both;
    }
    .hero::after {
      content: "";
      position: absolute;
      bottom: 0;
      left: var(--hu-space-lg);
      right: var(--hu-space-lg);
      height: 1px;
      background: linear-gradient(
        90deg,
        transparent,
        var(--hu-border-subtle) 20%,
        var(--hu-border-subtle) 80%,
        transparent
      );
    }
    .mesh {
      position: absolute;
      inset: 0;
      overflow: hidden;
      background:
        radial-gradient(
          ellipse at 20% 50%,
          color-mix(in srgb, var(--hu-dynamic-primary-800) 8%, transparent),
          transparent 60%
        ),
        radial-gradient(
          ellipse at 80% 30%,
          color-mix(in srgb, var(--hu-dynamic-tertiary-800) 6%, transparent),
          transparent 50%
        ),
        var(--hu-bg);
      pointer-events: none;
    }
    .shimmer {
      position: absolute;
      inset: -20%;
      background: radial-gradient(
        ellipse at 50% 50%,
        color-mix(in srgb, var(--hu-accent) 6%, transparent),
        transparent 70%
      );
      pointer-events: none;
      animation: hu-hero-shimmer 8s var(--hu-ease-in-out) infinite; /* hu-lint-ok: ambient shimmer cycle */
      will-change: transform, opacity;
    }
    .content {
      position: relative;
      z-index: 1;
    }
    @media (prefers-reduced-motion: reduce) {
      .hero {
        animation: none;
      }
      .shimmer {
        animation: none;
        opacity: 0.7;
      }
    }
  `;

  override render() {
    return html`
      <div class="hero">
        <div class="mesh" aria-hidden="true">
          <div class="shimmer"></div>
        </div>
        <div class="content">
          <slot></slot>
        </div>
      </div>
    `;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "hu-page-hero": ScPageHero;
  }
}
