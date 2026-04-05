import { LitElement, html, css } from "lit";
import { customElement, property, state } from "lit/decorators.js";

export type DeliveryStatus = "sending" | "sent" | "streaming" | "complete" | "failed";
export type ErrorType = "network" | "rate_limit" | "server" | "auth";

const ARIA_LABELS: Record<DeliveryStatus, string> = {
  sending: "Sending",
  sent: "Sent",
  streaming: "Streaming response",
  complete: "Complete",
  failed: "Failed to send",
};

const ERROR_LABELS: Record<ErrorType, string> = {
  network: "Connection lost",
  rate_limit: "Rate limited",
  server: "Server error",
  auth: "Session expired",
};

const COUNTDOWN_CIRCUMFERENCE = 2 * Math.PI * 10;

@customElement("hu-delivery-status")
export class ScDeliveryStatus extends LitElement {
  @property({ type: String }) status: DeliveryStatus = "sent";
  @property({ type: String }) errorType: ErrorType = "server";

  @state() private _retryCount = 0;
  @state() private _countdown = 0;
  @state() private _totalBackoff = 0;
  private _countdownTimer = 0;

  static override styles = css`
    @keyframes hu-delivery-pulse {
      0%,
      100% {
        opacity: 1;
      }
      50% {
        opacity: 0.3;
      }
    }

    @keyframes hu-delivery-dots {
      0%,
      100% {
        opacity: 0.3;
      }
      50% {
        opacity: 1;
      }
    }

    @keyframes hu-delivery-bounce {
      0% {
        transform: scale(0.8);
      }
      70% {
        transform: scale(1.1);
      }
      100% {
        transform: scale(1);
      }
    }

    @keyframes hu-sent-glow {
      0% {
        transform: scale(1);
        filter: drop-shadow(0 0 0 transparent);
      }
      40% {
        transform: scale(1.15);
        filter: drop-shadow(0 0 4px color-mix(in srgb, var(--hu-accent) 40%, transparent));
      }
      100% {
        transform: scale(1);
        filter: drop-shadow(0 0 0 transparent);
      }
    }

    :host {
      display: inline-flex;
      align-items: center;
      gap: var(--hu-space-2xs);
      font-size: var(--hu-text-2xs, 10px);
      font-family: var(--hu-font);
      color: var(--hu-text-faint);
    }

    .icon {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      width: var(--hu-text-xs);
      height: var(--hu-text-xs);
      flex-shrink: 0;
    }

    .icon svg {
      width: 100%;
      height: 100%;
      fill: currentColor;
    }

    /* Sending: pulsing dot */
    .dot {
      width: var(--hu-space-2xs);
      height: var(--hu-space-2xs);
      border-radius: 50%;
      background: currentColor;
      animation: hu-delivery-pulse var(--hu-duration-slowest) var(--hu-ease-in-out) infinite;
    }

    /* Streaming: three sequenced dots */
    .stream-dots {
      display: inline-flex;
      align-items: center;
      gap: var(--hu-space-2xs);
    }

    .stream-dots .dot {
      width: var(--hu-space-2xs);
      height: var(--hu-space-2xs);
      border-radius: 50%;
      background: currentColor;
      animation: hu-delivery-dots var(--hu-duration-slowest) var(--hu-ease-in-out) infinite;
    }

    .stream-dots .dot:nth-child(1) {
      animation-delay: 0ms;
    }

    .stream-dots .dot:nth-child(2) {
      animation-delay: var(--hu-duration-fast);
    }

    .stream-dots .dot:nth-child(3) {
      animation-delay: calc(var(--hu-duration-fast) * 2);
    }

    :host([data-status="streaming"]) {
      color: var(--hu-accent-tertiary-text);
    }

    :host([data-status="sent"]) .icon {
      animation: hu-sent-glow var(--hu-duration-moderate)
        var(--hu-ease-spring, cubic-bezier(0.34, 1.56, 0.64, 1)) 1;
    }

    :host([data-status="complete"]) {
      color: var(--hu-accent-tertiary-text);
    }

    :host([data-status="complete"]) .icon {
      animation: hu-delivery-bounce var(--hu-duration-moderate) var(--hu-ease-out) 1;
    }

    /* Error type color overrides */
    :host([data-status="failed"]) {
      color: var(--hu-error);
    }

    :host([data-error-type="network"]) {
      color: var(--hu-warning);
    }

    :host([data-error-type="rate_limit"]) {
      color: var(--hu-warning);
    }

    .retry {
      background: none;
      border: none;
      padding: 0;
      margin: 0;
      font: inherit;
      color: inherit;
      text-decoration: underline;
      cursor: pointer;
      font-size: inherit;
    }

    .retry:hover {
      text-decoration: none;
    }

    .retry:focus-visible {
      outline: var(--hu-focus-ring-width) solid var(--hu-focus-ring);
      outline-offset: var(--hu-focus-ring-offset);
    }

    .wrap {
      display: inline-flex;
      align-items: center;
      gap: var(--hu-space-2xs);
    }

    .countdown-container {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      position: relative;
      width: var(--hu-text-sm);
      height: var(--hu-text-sm);
      flex-shrink: 0;
    }

    .countdown-arc {
      width: 100%;
      height: 100%;
    }

    .countdown-arc circle {
      fill: none;
      stroke: currentColor;
      stroke-width: 2.5;
      stroke-linecap: round;
      transition: stroke-dashoffset 1s linear;
    }

    .countdown-text {
      position: absolute;
      font-size: var(--hu-text-2xs, 10px);
      font-variant-numeric: tabular-nums;
      line-height: 1;
    }

    .error-message {
      white-space: nowrap;
    }

    @media (prefers-reduced-motion: reduce) {
      .dot,
      .stream-dots .dot {
        animation: none;
      }

      :host([data-status="sent"]) .icon,
      :host([data-status="complete"]) .icon {
        animation: none;
      }

      .countdown-arc circle {
        transition: none;
      }
    }
  `;

  override updated(changed: Map<string, unknown>): void {
    if (changed.has("status") || (changed.has("errorType") && this.status === "failed")) {
      if (this.status === "failed") {
        this._startAutoRetry();
      } else {
        this._clearCountdown();
        if (this.status === "sent" || this.status === "complete" || this.status === "streaming") {
          this._retryCount = 0;
        }
      }
    }
  }

  override disconnectedCallback(): void {
    this._clearCountdown();
    super.disconnectedCallback();
  }

  private _clearCountdown(): void {
    if (this._countdownTimer) {
      clearInterval(this._countdownTimer);
      this._countdownTimer = 0;
    }
    this._countdown = 0;
  }

  private _startAutoRetry(): void {
    this._clearCountdown();
    if (this.errorType === "auth" || this._retryCount >= 3) return;

    const backoff = 3 * Math.pow(2, this._retryCount);
    this._totalBackoff = backoff;
    this._countdown = backoff;

    this._countdownTimer = window.setInterval(() => {
      this._countdown--;
      if (this._countdown <= 0) {
        this._clearCountdown();
        this._retryCount++;
        this.dispatchEvent(
          new CustomEvent("hu-retry", { bubbles: true, composed: true }),
        );
      }
    }, 1000);
  }

  private _onRetry(e: Event) {
    e.preventDefault();
    this._retryCount = 0;
    this._clearCountdown();
    this.dispatchEvent(
      new CustomEvent("hu-retry", { bubbles: true, composed: true }),
    );
  }

  private _onLogin(e: Event) {
    e.preventDefault();
    this.dispatchEvent(
      new CustomEvent("hu-login", { bubbles: true, composed: true }),
    );
  }

  private _renderFailed() {
    const isAuth = this.errorType === "auth";
    const exhausted = this._retryCount >= 3;
    const autoRetrying = !isAuth && !exhausted && this._countdown > 0;
    const label = ERROR_LABELS[this.errorType] ?? "Failed to send";

    if (autoRetrying) {
      const fraction = this._totalBackoff > 0 ? this._countdown / this._totalBackoff : 0;
      const offset = COUNTDOWN_CIRCUMFERENCE * (1 - fraction);

      return html`
        <span role="status" aria-live="polite" class="wrap">
          <span class="countdown-container">
            <svg class="countdown-arc" viewBox="0 0 24 24" aria-hidden="true">
              <circle
                cx="12"
                cy="12"
                r="10"
                stroke-dasharray="${COUNTDOWN_CIRCUMFERENCE}"
                stroke-dashoffset="${offset}"
                transform="rotate(-90 12 12)"
              />
            </svg>
            <span class="countdown-text" aria-hidden="true">${this._countdown}</span>
          </span>
          <span class="error-message"
            >${label} &middot; Retrying in ${this._countdown}s</span
          >
        </span>
      `;
    }

    if (isAuth) {
      return html`
        <span role="status" aria-live="assertive" class="wrap">
          <span class="icon">
            <svg viewBox="0 0 256 256" aria-hidden="true">
              <path
                d="M208.49,192.49a12,12,0,0,1-17,17L128,145,64.49,208.49a12,12,0,0,1-17-17L111,128,47.51,64.49a12,12,0,0,1,17-17L128,111l63.51-63.52a12,12,0,0,1,17,17L145,128Z"
                fill="currentColor"
              />
            </svg>
          </span>
          <span class="error-message">${label}</span>
          <button type="button" class="retry" @click=${this._onLogin} aria-label="Log in">
            Log in
          </button>
        </span>
      `;
    }

    return html`
      <span role="status" aria-live="polite" class="wrap">
        <span class="icon">
          <svg viewBox="0 0 256 256" aria-hidden="true">
            <path
              d="M208.49,192.49a12,12,0,0,1-17,17L128,145,64.49,208.49a12,12,0,0,1-17-17L111,128,47.51,64.49a12,12,0,0,1,17-17L128,111l63.51-63.52a12,12,0,0,1,17,17L145,128Z"
              fill="currentColor"
            />
          </svg>
        </span>
        <span class="error-message">${label}</span>
        <button type="button" class="retry" @click=${this._onRetry} aria-label="Retry sending">
          Retry
        </button>
      </span>
    `;
  }

  override render() {
    this.setAttribute("data-status", this.status);
    this.setAttribute("aria-label", ARIA_LABELS[this.status]);
    if (this.status === "failed") {
      this.setAttribute("data-error-type", this.errorType);
    } else {
      this.removeAttribute("data-error-type");
    }

    switch (this.status) {
      case "sending":
        return html`
          <span role="status" aria-live="polite" class="icon">
            <span class="dot" aria-hidden="true"></span>
          </span>
        `;

      case "sent":
        return html`
          <span role="status" aria-live="polite" class="icon">
            <svg viewBox="0 0 256 256" aria-hidden="true">
              <path
                d="M229.66,77.66l-128,128a8,8,0,0,1-11.32,0l-56-56a8,8,0,0,1,11.32-11.32L96,188.69,218.34,66.34a8,8,0,0,1,11.32,11.32Z"
                fill="currentColor"
              />
            </svg>
          </span>
        `;

      case "streaming":
        return html`
          <span role="status" aria-live="polite" class="stream-dots">
            <span class="dot" aria-hidden="true"></span>
            <span class="dot" aria-hidden="true"></span>
            <span class="dot" aria-hidden="true"></span>
          </span>
        `;

      case "complete":
        return html`
          <span role="status" aria-live="polite" class="icon">
            <svg viewBox="0 0 24 24" aria-hidden="true">
              <path
                d="M18 7l-1.41-1.41-6.34 6.34 1.41 1.41L18 7zm-4.24-.29l-5.66 5.66 1.41 1.41 5.66-5.66-1.41-1.41zM12 19.07l-5.66-5.66 1.41-1.41L12 16.24l5.66-5.66 1.41 1.41L12 19.07z"
                fill="currentColor"
              />
            </svg>
          </span>
        `;

      case "failed":
        return this._renderFailed();

      default:
        return html`<span role="status" aria-live="polite"></span>`;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "hu-delivery-status": ScDeliveryStatus;
  }
}
