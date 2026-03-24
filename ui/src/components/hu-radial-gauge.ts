import { LitElement, html, css, nothing } from "lit";
import { customElement, property, state } from "lit/decorators.js";

@customElement("hu-radial-gauge")
export class HuRadialGauge extends LitElement {
  @property({ type: Number }) value = 0;
  @property({ type: Number }) max = 100;
  @property({ type: String }) unit = "%";
  @property({ type: String }) label = "";
  @property({ type: Boolean }) glass = false;
  /** When set, arc color follows ratio: success / warning / error */
  @property({ attribute: false }) thresholds: { warn: number; danger: number } | null = {
    warn: 0.7,
    danger: 0.9,
  };

  @state() private _mounted = false;
  @state() private _reducedMotion = false;

  static override styles = css`
    :host {
      display: inline-block;
    }
    .wrap {
      position: relative;
      width: 100%;
      max-width: 160px;
    }
    .wrap.glass {
      padding: var(--hu-space-md);
      border-radius: var(--hu-radius-lg);
      background: color-mix(in srgb, var(--hu-surface-container) 72%, transparent);
      backdrop-filter: blur(var(--hu-glass-subtle-blur, 8px));
      -webkit-backdrop-filter: blur(var(--hu-glass-subtle-blur, 8px));
      border: 1px solid var(--hu-border-subtle);
    }
    svg {
      display: block;
      width: 100%;
      height: auto;
    }
    .track {
      fill: none;
      stroke: var(--hu-border-subtle);
      opacity: 0.55;
    }
    .value-arc {
      fill: none;
      stroke-linecap: round;
      transform: rotate(-90deg);
      transform-origin: center;
      transition: stroke-dashoffset var(--hu-duration-slower, 450ms)
        var(--hu-ease-out, cubic-bezier(0.22, 1, 0.36, 1));
    }
    .center {
      position: absolute;
      inset: 0;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      pointer-events: none;
      font-family: var(--hu-font);
    }
    .center-val {
      font-size: var(--hu-text-xl);
      font-weight: var(--hu-weight-bold);
      color: var(--hu-text);
      font-variant-numeric: tabular-nums;
    }
    .center-unit {
      font-size: var(--hu-text-sm);
      color: var(--hu-text-muted);
    }
    .center-label {
      font-size: var(--hu-text-2xs);
      color: var(--hu-text-muted);
      margin-top: var(--hu-space-2xs);
      text-align: center;
      max-width: 90%;
    }
    @media (prefers-reduced-motion: reduce) {
      .value-arc {
        transition: none;
      }
    }
  `;

  override connectedCallback(): void {
    super.connectedCallback();
    this._reducedMotion = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  }

  private _strokeColor(ratio: number): string {
    const t = this.thresholds;
    if (!t) return "var(--hu-chart-brand)";
    if (ratio >= t.danger) return "var(--hu-error)";
    if (ratio >= t.warn) return "var(--hu-warning)";
    return "var(--hu-success)";
  }

  override render() {
    const vb = 120;
    const cx = vb / 2;
    const cy = vb / 2;
    const r = 44;
    const sw = 8;
    const c = 2 * Math.PI * r;
    const ratio = Math.min(Math.max(this.value / Math.max(this.max, 1e-9), 0), 1);
    const offset = c * (1 - ratio);
    const dashOff = this._mounted || this._reducedMotion ? offset : c;
    const pct = Math.round(ratio * 100);
    const color = this._strokeColor(ratio);

    return html`
      <div
        class="wrap ${this.glass ? "glass" : ""}"
        role="img"
        aria-label=${this.label || `Gauge ${pct}${this.unit}`}
      >
        <svg viewBox="0 0 ${vb} ${vb}" aria-hidden="true">
          <circle class="track" cx=${cx} cy=${cy} r=${r} stroke-width=${sw} />
          <circle
            class="value-arc"
            cx=${cx}
            cy=${cy}
            r=${r}
            stroke-width=${sw}
            stroke=${color}
            stroke-dasharray=${c}
            stroke-dashoffset=${dashOff}
          />
        </svg>
        <div class="center">
          <span class="center-val">${pct}</span>
          <span class="center-unit">${this.unit}</span>
          ${this.label ? html`<span class="center-label">${this.label}</span>` : nothing}
        </div>
      </div>
    `;
  }

  override firstUpdated(): void {
    if (!this._reducedMotion) {
      requestAnimationFrame(() => {
        this._mounted = true;
        this.requestUpdate();
      });
    } else {
      this._mounted = true;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "hu-radial-gauge": HuRadialGauge;
  }
}
