import { LitElement, html, css } from "lit";
import { customElement } from "lit/decorators.js";

@customElement("sc-page-hero")
export class ScPageHero extends LitElement {
  static override styles = css`
    :host {
      display: block;
    }
    .hero {
      position: relative;
      padding: var(--sc-space-2xl) var(--sc-space-lg) var(--sc-space-xl);
      overflow: hidden;
      animation: sc-fade-in 300ms var(--sc-ease-out) both;
    }
    .hero::after {
      content: "";
      position: absolute;
      bottom: 0;
      left: var(--sc-space-lg);
      right: var(--sc-space-lg);
      height: 1px;
      background: linear-gradient(
        90deg,
        transparent,
        var(--sc-border-subtle) 20%,
        var(--sc-border-subtle) 80%,
        transparent
      );
    }
    .mesh {
      position: absolute;
      inset: 0;
      background:
        radial-gradient(
          ellipse 80% 60% at 20% 40%,
          color-mix(in srgb, var(--sc-accent) 6%, transparent),
          transparent
        ),
        radial-gradient(
          ellipse 60% 50% at 80% 30%,
          color-mix(in srgb, var(--sc-accent-secondary, var(--sc-accent)) 4%, transparent),
          transparent
        );
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
    "sc-page-hero": ScPageHero;
  }
}
