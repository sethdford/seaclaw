import { LitElement, html, css } from "lit";
import { customElement } from "lit/decorators.js";

@customElement("hu-page-hero")
export class ScPageHero extends LitElement {
  static override styles = css`
    :host {
      display: block;
      view-transition-name: hu-page-hero;
    }
    .hero {
      position: relative;
      padding: var(--hu-space-2xl) var(--hu-space-lg) var(--hu-space-xl);
      overflow: hidden;
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
    .content {
      position: relative;
      z-index: 1;
    }
    @media (prefers-reduced-motion: reduce) {
      .hero {
        animation: none;
      }
    }
  `;

  override render() {
    return html`
      <div class="hero">
        <div class="mesh" aria-hidden="true"></div>
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
