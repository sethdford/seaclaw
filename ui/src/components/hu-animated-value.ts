import { LitElement, html, css, nothing } from "lit";
import { customElement, property, state } from "lit/decorators.js";
import { generateSpringKeyframes, SPRING_PRESETS } from "../utils/spring.js";

export type AnimatedValueFormat = "standard" | "compact" | "currency";

@customElement("hu-animated-value")
export class HuAnimatedValue extends LitElement {
  @property({ type: Number }) value = 0;
  @property({ type: String }) format: AnimatedValueFormat = "standard";
  @property({ type: String }) prefix = "";
  @property({ type: String }) suffix = "";
  @property({ type: String }) currency = "USD";
  @property({ type: Boolean }) showDelta = false;
  @property({ type: Number }) duration = 500;

  @state() private _displayed = 0;
  /** Previous committed `value` prop for delta display */
  @state() private _baseline = 0;

  private _raf = 0;
  private _start = 0;
  private _from = 0;
  /** One-time sync so first paint matches `value` (avoids flash 0 + extra update). */
  private _hydrated = false;
  /** Skip animating the initial Lit commit (distinct from later user-driven updates). */
  private _valueSeenInUpdated = false;

  static override styles = css`
    :host {
      display: inline-flex;
      align-items: baseline;
      gap: var(--hu-space-xs);
      font-variant-numeric: tabular-nums;
      font-family: var(--hu-font);
    }
    .value {
      font-weight: var(--hu-weight-semibold);
      color: var(--hu-text);
    }
    .delta {
      font-size: var(--hu-text-xs);
      font-weight: var(--hu-weight-medium);
      border-radius: var(--hu-radius-sm);
      padding: var(--hu-space-2xs) var(--hu-space-xs);
    }
    .delta.up {
      color: var(--hu-success);
      background: var(--hu-success-dim);
    }
    .delta.down {
      color: var(--hu-error);
      background: var(--hu-error-dim);
    }
    .delta.flat {
      color: var(--hu-text-muted);
      background: var(--hu-bg-elevated);
    }
  `;

  override willUpdate(_changed: Map<string, unknown>): void {
    if (!this._hydrated) {
      this._hydrated = true;
      this._displayed = this.value;
      this._baseline = this.value;
    }
  }

  private _formatter(): Intl.NumberFormat {
    if (this.format === "compact") {
      return new Intl.NumberFormat(undefined, { notation: "compact", maximumFractionDigits: 1 });
    }
    if (this.format === "currency") {
      return new Intl.NumberFormat(undefined, { style: "currency", currency: this.currency });
    }
    return new Intl.NumberFormat(undefined, { maximumFractionDigits: 2 });
  }

  override updated(changed: Map<string, unknown>): void {
    if (!changed.has("value")) return;
    const prevProp = changed.get("value") as number;
    if (!this._valueSeenInUpdated) {
      this._valueSeenInUpdated = true;
      this._displayed = this.value;
      this._baseline = this.value;
      return;
    }
    if (this.showDelta) {
      this._baseline = prevProp;
    }
    this._animate(this._displayed, this.value);
  }

  private _animate(from: number, to: number): void {
    if (from === to) {
      this._displayed = to;
      return;
    }
    if (this._raf) cancelAnimationFrame(this._raf);
    if (window.matchMedia("(prefers-reduced-motion: reduce)").matches) {
      this._displayed = to;
      return;
    }
    this._from = from;
    this._start = performance.now();
    const keyframes = generateSpringKeyframes(SPRING_PRESETS.standard);
    const tick = (now: number) => {
      const elapsed = now - this._start;
      const t = Math.min(elapsed / this.duration, 1);
      const idx = t * (keyframes.length - 1);
      const i0 = Math.floor(idx);
      const i1 = Math.min(i0 + 1, keyframes.length - 1);
      const frac = idx - i0;
      const eased = keyframes[i0] + frac * (keyframes[i1] - keyframes[i0]);
      this._displayed = this._from + (to - this._from) * eased;
      if (t < 1) {
        this._raf = requestAnimationFrame(tick);
      } else {
        this._displayed = to;
      }
    };
    this._raf = requestAnimationFrame(tick);
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    if (this._raf) cancelAnimationFrame(this._raf);
  }

  private _deltaHtml() {
    if (!this.showDelta) return nothing;
    const prev = this._baseline;
    const delta = this.value - prev;
    if (delta === 0 && prev === this.value) return nothing;
    if (delta === 0) {
      return html`<span class="delta flat" aria-hidden="true">—</span>`;
    }
    const pct = prev !== 0 ? Math.round((delta / Math.abs(prev)) * 100) : 100;
    const up = delta > 0;
    return html`
      <span
        class="delta ${up ? "up" : "down"}"
        aria-label="${up ? "Increased" : "Decreased"} by ${Math.abs(pct)} percent"
      >
        ${up ? "↑" : "↓"} ${Math.abs(pct)}%
      </span>
    `;
  }

  override render() {
    const fmt = this._formatter();
    const text = fmt.format(this._displayed);
    return html`
      <span class="value" role="status" aria-live="polite"
        >${this.prefix}${text}${this.suffix}</span
      >
      ${this._deltaHtml()}
    `;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "hu-animated-value": HuAnimatedValue;
  }
}
