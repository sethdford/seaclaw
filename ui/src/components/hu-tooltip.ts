import { LitElement, html, css } from "lit";
import { customElement, property, state } from "lit/decorators.js";

type TooltipPosition = "top" | "bottom" | "left" | "right";

let tooltipIdCounter = 0;

@customElement("hu-tooltip")
export class ScTooltip extends LitElement {
  static override styles = css`
    :host {
      display: inline-block;
      position: relative;
    }

    .wrapper {
      position: relative;
      display: inline-block;
    }

    .tip {
      position: absolute;
      white-space: nowrap;
      background: color-mix(in srgb, var(--hu-bg-overlay) 90%, transparent);
      backdrop-filter: blur(var(--hu-glass-subtle-blur, 8px))
        saturate(var(--hu-glass-subtle-saturate, 1.2));
      -webkit-backdrop-filter: blur(var(--hu-glass-subtle-blur, 8px))
        saturate(var(--hu-glass-subtle-saturate, 1.2));
      color: var(--hu-text);
      font-size: var(--hu-text-xs);
      padding: var(--hu-space-xs) var(--hu-space-sm);
      border-radius: var(--hu-radius-sm);
      border: 1px solid var(--hu-glass-border-color, var(--hu-border-subtle));
      box-shadow: var(--hu-shadow-md);
      pointer-events: none;
      opacity: 0;
      visibility: hidden;
      transform-origin: center;
      z-index: 1000;
    }

    :host(:hover) .tip,
    :host(:focus-within) .tip {
      opacity: 1;
      visibility: visible;
      animation: hu-scale-in var(--hu-duration-fast) var(--hu-spring-out);
    }

    .tip.top {
      bottom: 100%;
      left: 50%;
      transform: translateX(-50%);
      margin-bottom: var(--hu-space-xs);
    }

    .tip.bottom {
      top: 100%;
      left: 50%;
      transform: translateX(-50%);
      margin-top: var(--hu-space-xs);
    }

    .tip.left {
      right: 100%;
      top: 50%;
      transform: translateY(-50%);
      margin-right: var(--hu-space-xs);
    }

    .tip.right {
      left: 100%;
      top: 50%;
      transform: translateY(-50%);
      margin-left: var(--hu-space-xs);
    }

    @media (prefers-reduced-motion: reduce) {
      :host(:hover) .tip,
      :host(:focus-within) .tip {
        animation: none;
      }
    }

    @media (prefers-reduced-transparency: reduce) {
      .tip {
        backdrop-filter: none;
        -webkit-backdrop-filter: none;
        background: var(--hu-bg-overlay);
      }
    }

    /* CSS Anchor Positioning — progressive enhancement (default top + other positions). */
    @supports (anchor-name: --test) {
      :host {
        anchor-name: --hu-tooltip-anchor;
      }

      .tooltip-content {
        position-anchor: --hu-tooltip-anchor;
        position: fixed;
        position-try-fallbacks: flip-block, flip-inline;
        bottom: auto;
        left: auto;
        right: auto;
        top: auto;
        transform: none;
        margin: 0;
      }

      :host([position="top"]) .tooltip-content {
        inset-area: block-start;
        margin-block-end: var(--hu-space-xs);
      }

      :host([position="bottom"]) .tooltip-content {
        inset-area: block-end;
        margin-block-start: var(--hu-space-xs);
      }

      :host([position="left"]) .tooltip-content {
        inset-area: inline-start;
        margin-inline-end: var(--hu-space-xs);
      }

      :host([position="right"]) .tooltip-content {
        inset-area: inline-end;
        margin-inline-start: var(--hu-space-xs);
      }
    }
  `;

  @property({ type: String }) text = "";
  @property({ type: String, reflect: true }) position: TooltipPosition = "top";
  @state() private _tipId = `hu-tip-${tooltipIdCounter++}`;

  override render() {
    return html`
      <div class="wrapper" aria-describedby=${this._tipId}>
        <slot></slot>
        <div id=${this._tipId} class="tip tooltip-content ${this.position}" role="tooltip">
          ${this.text}
        </div>
      </div>
    `;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "hu-tooltip": ScTooltip;
  }
}
