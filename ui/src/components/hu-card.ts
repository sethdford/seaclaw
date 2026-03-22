import { LitElement, html, css } from "lit";
import { customElement, property } from "lit/decorators.js";
import { pointerProximity } from "../lib/pointer-proximity.js";

export type CardSurface = "default" | "high" | "highest";

@customElement("hu-card")
export class ScCard extends LitElement {
  @property({ type: Boolean, reflect: true }) hoverable = false;
  @property({ type: Boolean, reflect: true }) clickable = false;
  @property({ type: Boolean }) accent = false;
  @property({ type: Boolean }) elevated = false;
  @property({ type: Boolean }) glass = true;
  @property({ type: Boolean }) solid = false;
  /** Enable 3D perspective tilt on pointer proximity. */
  @property({ type: Boolean }) tilt = false;
  /** Enable mesh gradient background overlay. */
  @property({ type: Boolean }) mesh = false;
  /** Enable chromatic prismatic border. */
  @property({ type: Boolean }) chromatic = false;
  /** Animate card into view when it intersects the viewport (IntersectionObserver). */
  @property({ type: Boolean, reflect: true }) animate = false;
  /** Tonal surface: default (container), high (interactive/elevated), highest (emphasis). */
  @property({ type: String }) surface: CardSurface = "default";

  static override styles = css`
    :host {
      display: block;
    }

    .card {
      position: relative;
      background: var(--hu-card-surface, var(--hu-surface-container));
      background-image: var(--hu-surface-gradient);
      border: 1px solid var(--hu-border-subtle);
      border-radius: var(--hu-radius-xl);
      padding: var(--hu-space-xl);
      box-shadow:
        var(--hu-shadow-card),
        inset 0 1px 0 color-mix(in srgb, var(--hu-color-white) 90%, transparent),
        inset 0 -1px 0 color-mix(in srgb, var(--hu-text) 4%, transparent);
      overflow: hidden;
      container-type: inline-size;
      contain: layout style paint;
    }

    /* Gradient border glow — bright top edge fading to transparent bottom (Apple Liquid Glass) */
    .card::before {
      content: "";
      position: absolute;
      inset: 0;
      border-radius: inherit;
      background: linear-gradient(
        180deg,
        color-mix(in srgb, var(--hu-color-white) 70%, transparent),
        color-mix(in srgb, var(--hu-color-white) 10%, transparent) 30%,
        transparent 60%
      );
      mask:
        linear-gradient(var(--hu-color-white) 0 0) content-box,
        linear-gradient(var(--hu-color-white) 0 0);
      mask-composite: exclude;
      -webkit-mask:
        linear-gradient(var(--hu-color-white) 0 0) content-box,
        linear-gradient(var(--hu-color-white) 0 0);
      -webkit-mask-composite: xor;
      padding: 1px;
      pointer-events: none;
      z-index: 1;
    }

    /* Ambient teal surface glow from top */
    .card::after {
      content: "";
      position: absolute;
      top: 0;
      left: 0;
      right: 0;
      height: 6.25rem;
      background: radial-gradient(
        ellipse 90% 70% at 50% -20%,
        color-mix(in srgb, var(--hu-accent) 6%, transparent),
        transparent
      );
      border-radius: inherit;
      pointer-events: none;
      z-index: 0;
    }

    .card > ::slotted(*),
    .card > * {
      position: relative;
      z-index: 2;
    }

    .card.elevated {
      box-shadow:
        var(--hu-shadow-md),
        inset 0 1px 0 color-mix(in srgb, var(--hu-color-white) 90%, transparent),
        inset 0 -1px 0 color-mix(in srgb, var(--hu-text) 4%, transparent);
    }

    /* Accent top-band — teal gradient bar + tinted wash below */
    .card.accent {
      border-top: none;
      padding-top: calc(var(--hu-space-xl) + var(--hu-space-xs));
    }
    .card.accent::after {
      height: var(--hu-space-xs);
      top: 0;
      background: linear-gradient(
        90deg,
        var(--hu-accent),
        color-mix(in srgb, var(--hu-accent) 40%, transparent)
      );
      border-radius: var(--hu-radius-xl) var(--hu-radius-xl) 0 0;
    }
    .card.accent::before {
      background: linear-gradient(
        180deg,
        color-mix(in srgb, var(--hu-accent) 6%, transparent),
        transparent 40%
      );
      mask: none;
      -webkit-mask: none;
      padding: 0;
    }

    /* Glass variant — Apple Liquid Glass with specular highlight */
    .card.glass {
      background: color-mix(
        in srgb,
        var(--hu-card-surface, var(--hu-surface-container)) var(--hu-glass-card-bg-mix, 65%),
        transparent
      );
      backdrop-filter: blur(var(--hu-blur-lg)) saturate(var(--hu-glass-standard-saturate));
      -webkit-backdrop-filter: blur(var(--hu-blur-lg)) saturate(var(--hu-glass-standard-saturate));
      border: 1px solid color-mix(in srgb, var(--hu-border-subtle) 40%, transparent);
      box-shadow:
        var(--hu-shadow-card),
        inset 0 1px 0 color-mix(in srgb, var(--hu-color-white) 60%, transparent),
        inset 0 -1px 0 color-mix(in srgb, var(--hu-text) 3%, transparent);
    }
    .card.glass::before {
      background: radial-gradient(
        ellipse 60% 40% at 25% 0%,
        color-mix(in srgb, var(--hu-color-white) 15%, transparent),
        transparent 70%
      );
      mask: none;
      -webkit-mask: none;
      padding: 0;
    }

    .card.hoverable,
    .card.clickable {
      cursor: pointer;
      will-change: transform, box-shadow;
    }
    :host([hoverable]),
    :host([clickable]) {
      transition:
        transform var(--hu-duration-normal) var(--hu-ease-spring),
        box-shadow var(--hu-duration-normal) var(--hu-ease-out);
    }
    :host([hoverable]:hover),
    :host([clickable]:hover) {
      transform: translateY(var(--hu-physics-card-hover-translateY, -2px));
      box-shadow: var(--hu-shadow-lg);
    }
    :host([hoverable]:active),
    :host([clickable]:active) {
      transform: translateY(0) scale(0.99);
    }
    .card.clickable:focus-visible {
      outline: var(--hu-focus-ring-width) solid var(--hu-focus-ring);
      outline-offset: var(--hu-focus-ring-offset);
    }

    /* 3D perspective tilt — pointer proximity driven */
    :host([tilt]) {
      perspective: 1200px;
    }
    :host([tilt]) .card {
      transform-style: preserve-3d;
      transform: rotateY(calc(var(--hu-pointer-x, 0px) * 0.02deg))
        rotateX(calc(var(--hu-pointer-y, 0px) * -0.02deg));
      transition: transform var(--hu-duration-fast) var(--hu-spring-out);
    }
    /* Pointer proximity glow overlay */
    :host([tilt]) .card .proximity-glow {
      position: absolute;
      inset: 0;
      border-radius: inherit;
      pointer-events: none;
      z-index: 1;
      opacity: var(--hu-proximity, 0);
      background: radial-gradient(
        200px at calc(50% + var(--hu-pointer-x, 0px)) calc(50% + var(--hu-pointer-y, 0px)),
        color-mix(in srgb, var(--hu-accent) 6%, transparent),
        transparent
      );
      transition: opacity var(--hu-duration-fast) var(--hu-ease-out);
    }

    /* Mesh gradient background */
    .card.mesh {
      background-image:
        radial-gradient(
          ellipse at 20% 50%,
          color-mix(in srgb, var(--hu-accent) 6%, transparent) 0%,
          transparent 50%
        ),
        radial-gradient(
          ellipse at 80% 20%,
          color-mix(in srgb, var(--hu-accent-tertiary) 4%, transparent) 0%,
          transparent 50%
        ),
        radial-gradient(
          ellipse at 50% 80%,
          color-mix(in srgb, var(--hu-accent-secondary) 3%, transparent) 0%,
          transparent 50%
        ),
        var(--hu-surface-gradient);
    }

    /* Chromatic prismatic border */
    .card.chromatic::before {
      background: linear-gradient(
        135deg,
        color-mix(in srgb, var(--hu-accent) 15%, transparent),
        color-mix(in srgb, var(--hu-accent-tertiary) 12%, transparent),
        color-mix(in srgb, var(--hu-accent-secondary) 10%, transparent),
        color-mix(in srgb, var(--hu-accent) 15%, transparent)
      );
      mask:
        linear-gradient(var(--hu-color-white) 0 0) content-box,
        linear-gradient(var(--hu-color-white) 0 0);
      mask-composite: exclude;
      -webkit-mask:
        linear-gradient(var(--hu-color-white) 0 0) content-box,
        linear-gradient(var(--hu-color-white) 0 0);
      -webkit-mask-composite: xor;
      padding: 1px;
    }

    :host([animate]) .card {
      opacity: 0;
      transform: translateY(var(--hu-space-sm, 8px));
      transition:
        opacity var(--hu-duration-normal, 250ms) var(--hu-ease-out),
        transform var(--hu-duration-normal, 250ms) var(--hu-ease-out);
      transition-delay: var(--hu-stagger-delay, 0ms);
    }

    :host([animate].entered) .card {
      opacity: 1;
      transform: translateY(0);
    }

    @media (prefers-reduced-motion: reduce) {
      :host([animate]) .card {
        opacity: 1;
        transform: none;
        transition: none;
      }
    }

    @media (prefers-reduced-motion: reduce) {
      :host([hoverable]),
      :host([clickable]),
      :host([tilt]) .card {
        transition: none !important;
        animation: none !important;
        transform: none !important;
      }
    }

    @container (max-width: 280px) {
      .card {
        padding: var(--hu-space-md);
      }
    }

    @media (prefers-contrast: more) {
      .card {
        border-width: 2px;
        border-color: var(--hu-border);
        box-shadow: none;
      }
    }
  `;

  private _entranceObserver: IntersectionObserver | null = null;

  private _teardownEntranceObserver(): void {
    if (this._entranceObserver) {
      this._entranceObserver.disconnect();
      this._entranceObserver = null;
    }
  }

  private _setupEntranceObserver(): void {
    this._teardownEntranceObserver();
    if (!this.animate) {
      return;
    }
    if (typeof IntersectionObserver === "undefined") {
      this.classList.add("entered");
      return;
    }
    this._entranceObserver = new IntersectionObserver(
      (entries) => {
        for (const entry of entries) {
          if (entry.isIntersecting) {
            this.classList.add("entered");
            this._teardownEntranceObserver();
            break;
          }
        }
      },
      { threshold: 0.1 },
    );
    this._entranceObserver.observe(this);
  }

  override connectedCallback(): void {
    super.connectedCallback();
    if (this.tilt) {
      pointerProximity.observe(this);
    }
    if (this.animate) {
      this._setupEntranceObserver();
    }
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    pointerProximity.unobserve(this);
    this._teardownEntranceObserver();
  }

  override updated(changed: Map<string, unknown>): void {
    if (changed.has("tilt")) {
      if (this.tilt) {
        pointerProximity.observe(this);
      } else {
        pointerProximity.unobserve(this);
      }
    }
    if (changed.has("animate")) {
      if (this.animate) {
        this._setupEntranceObserver();
      } else {
        this._teardownEntranceObserver();
        this.classList.remove("entered");
      }
    }
  }

  private _onKeyDown(e: KeyboardEvent): void {
    if (!this.clickable || (e.key !== "Enter" && e.key !== " ")) return;
    e.preventDefault();
    this.dispatchEvent(new MouseEvent("click", { bubbles: true, composed: true }));
  }

  render() {
    const surfaceToken =
      this.surface === "high"
        ? "var(--hu-surface-container-high)"
        : this.surface === "highest"
          ? "var(--hu-surface-container-highest)"
          : "var(--hu-surface-container)";

    const classes = [
      "card",
      this.hoverable ? "hoverable" : "",
      this.clickable ? "clickable" : "",
      this.accent ? "accent" : "",
      this.elevated ? "elevated" : "",
      !this.solid ? "glass" : "",
      this.mesh ? "mesh" : "",
      this.chromatic ? "chromatic" : "",
    ]
      .filter(Boolean)
      .join(" ");

    return html`
      <div
        class=${classes}
        style="--hu-card-surface: ${surfaceToken}"
        role=${this.clickable ? "button" : undefined}
        tabindex=${this.clickable ? 0 : undefined}
        @keydown=${this._onKeyDown}
      >
        ${this.tilt ? html`<div class="proximity-glow"></div>` : ""}
        <slot></slot>
      </div>
    `;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "hu-card": ScCard;
  }
}
