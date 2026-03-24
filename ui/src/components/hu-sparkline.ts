import { LitElement, html, css } from "lit";
import type { PropertyValues } from "lit";
import { customElement, property } from "lit/decorators.js";

@customElement("hu-sparkline")
export class ScSparkline extends LitElement {
  @property({ type: Array }) data: number[] = [];
  @property({ type: Number }) width = 80;
  @property({ type: Number }) height = 28;
  @property({ type: String }) color = "var(--hu-accent-tertiary)";
  @property({ type: Boolean }) fill = true;

  private _lastDrawSignature = "";
  private _drawAnimPath: SVGPathElement | null = null;
  private _drawAnimListener: ((e: AnimationEvent) => void) | null = null;

  static override styles = css`
    :host {
      display: inline-block;
      vertical-align: middle;
    }
    svg {
      display: block;
      overflow: visible;
    }
    .line {
      fill: none;
      stroke-width: 1.5;
      stroke-linecap: round;
      stroke-linejoin: round;
    }
    .line.hu-sparkline-draw {
      stroke-dasharray: var(--sparkline-path-length);
      stroke-dashoffset: var(--sparkline-path-length);
      animation: hu-sparkline-draw var(--hu-duration-slow) var(--hu-ease-out) forwards;
      animation-delay: var(
        --sparkline-delay,
        calc(var(--sparkline-index, 0) * var(--hu-stagger-delay))
      );
      will-change: stroke-dashoffset;
    }
    @keyframes hu-sparkline-draw {
      to {
        stroke-dashoffset: 0;
      }
    }
    @media (prefers-reduced-motion: reduce) {
      .line.hu-sparkline-draw {
        animation: none;
        stroke-dasharray: none;
        stroke-dashoffset: 0;
        will-change: auto;
      }
    }
    .area {
      opacity: 0.15;
    }
    .dot {
      filter: drop-shadow(0 0 2px currentColor);
    }
  `;

  private _buildPath(points: number[]): { line: string; area: string } {
    if (points.length < 2) return { line: "", area: "" };
    const min = Math.min(...points);
    const max = Math.max(...points);
    const range = max - min || 1;
    const pad = 2;
    const w = this.width - pad * 2;
    const h = this.height - pad * 2;

    const coords = points.map((v, i) => ({
      x: pad + (i / (points.length - 1)) * w,
      y: pad + h - ((v - min) / range) * h,
    }));

    const line = coords
      .map((c, i) => `${i === 0 ? "M" : "L"}${c.x.toFixed(1)},${c.y.toFixed(1)}`)
      .join(" ");
    const area = `${line} L${coords[coords.length - 1].x.toFixed(1)},${this.height.toFixed(1)} L${coords[0].x.toFixed(1)},${this.height.toFixed(1)} Z`;
    return { line, area };
  }

  private _drawSignature(): string {
    const pts = this.data.length >= 2 ? this.data : [0, 0];
    return `${this.width}:${this.height}:${pts.join(",")}`;
  }

  private _teardownLineDraw() {
    if (this._drawAnimPath && this._drawAnimListener) {
      this._drawAnimPath.removeEventListener("animationend", this._drawAnimListener);
    }
    if (this._drawAnimPath) {
      this._drawAnimPath.classList.remove("hu-sparkline-draw");
      this._drawAnimPath.style.willChange = "";
      this._drawAnimPath.style.removeProperty("--sparkline-path-length");
    }
    this._drawAnimPath = null;
    this._drawAnimListener = null;
  }

  private _applyLineDrawAnimation() {
    this._teardownLineDraw();

    const path = this.renderRoot.querySelector<SVGPathElement>(".line");
    if (!path) return;

    if (window.matchMedia("(prefers-reduced-motion: reduce)").matches) {
      return;
    }

    const len = path.getTotalLength();
    if (!Number.isFinite(len) || len <= 0) return;

    path.classList.remove("hu-sparkline-draw");
    path.style.removeProperty("--sparkline-path-length");
    void path.getBoundingClientRect();
    path.style.setProperty("--sparkline-path-length", String(len));

    requestAnimationFrame(() => {
      const again = this.renderRoot.querySelector<SVGPathElement>(".line");
      if (again !== path) return;

      path.style.willChange = "stroke-dashoffset";
      path.classList.add("hu-sparkline-draw");

      const onEnd = (ev: AnimationEvent) => {
        if (ev.target !== path) return;
        path.removeEventListener("animationend", onEnd);
        path.classList.remove("hu-sparkline-draw");
        path.style.willChange = "";
        path.style.removeProperty("--sparkline-path-length");
        this._drawAnimPath = null;
        this._drawAnimListener = null;
      };
      this._drawAnimPath = path;
      this._drawAnimListener = onEnd;
      path.addEventListener("animationend", onEnd);
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this._teardownLineDraw();
  }

  override updated(_changed: PropertyValues<this>) {
    super.updated(_changed);
    const sig = this._drawSignature();
    if (sig === this._lastDrawSignature) return;
    this._lastDrawSignature = sig;
    void this.updateComplete.then(() => this._applyLineDrawAnimation());
  }

  override render() {
    const pts = this.data.length >= 2 ? this.data : [0, 0];
    const { line, area } = this._buildPath(pts);
    const last = pts[pts.length - 1];
    const min = Math.min(...pts);
    const max = Math.max(...pts);
    const range = max - min || 1;
    const pad = 2;
    const h = this.height - pad * 2;
    const lastY = pad + h - ((last - min) / range) * h;
    const lastX = this.width - pad;
    const trend = last > pts[0] ? "upward" : last < pts[0] ? "downward" : "stable";
    const ariaLabel = `Sparkline: ${trend} trend over ${pts.length} data points`;

    return html`
      <svg
        width=${this.width}
        height=${this.height}
        viewBox="0 0 ${this.width} ${this.height}"
        role="img"
        aria-label=${ariaLabel}
      >
        ${this.fill ? html`<path class="area" d=${area} fill=${this.color} />` : null}
        <path class="line" d=${line} stroke=${this.color} />
        <circle class="dot" cx=${lastX} cy=${lastY} r="2.5" fill=${this.color} />
      </svg>
    `;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "hu-sparkline": ScSparkline;
  }
}
