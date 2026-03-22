import { LitElement, html, css } from "lit";
import { customElement, property } from "lit/decorators.js";
import { icons } from "../icons.js";

export type VoiceOrbState = "idle" | "listening" | "speaking" | "processing" | "unsupported";

@customElement("hu-voice-orb")
export class ScVoiceOrb extends LitElement {
  @property({ type: String }) state: VoiceOrbState = "idle";
  @property({ type: Number }) audioLevel = 0;
  @property({ type: Boolean }) disabled = false;

  static override styles = css`
    :host {
      display: flex;
      flex-direction: column;
      align-items: center;
      contain: layout style;
      container-type: inline-size;
      padding: var(--hu-space-lg) 0 var(--hu-space-md);
      position: relative;
      flex-shrink: 0;
    }

    .mic-orb-glow {
      position: absolute;
      top: 50%;
      left: 50%;
      transform: translate(-50%, -60%);
      width: 15rem;
      height: 15rem;
      border-radius: 50%;
      background: radial-gradient(circle, var(--hu-accent-subtle) 0%, transparent 70%);
      opacity: 0.4;
      pointer-events: none;
      transition:
        opacity var(--hu-duration-normal) var(--hu-ease-out),
        transform var(--hu-duration-fast) var(--hu-ease-spring, ease-out);
      &.active {
        opacity: calc(0.6 + var(--_level, 0) * 0.4);
        transform: translate(-50%, -60%) scale(calc(1 + var(--_level, 0) * 0.15));
      }
      &.speaking {
        background: radial-gradient(
          circle,
          color-mix(in srgb, var(--hu-accent-secondary) 45%, transparent) 0%,
          transparent 72%
        );
        opacity: 0.85;
        animation: hu-glow-speak 1.1s ease-in-out infinite; /* hu-lint-ok: ambient */
      }
      &.processing {
        background: radial-gradient(
          circle,
          color-mix(in srgb, var(--hu-accent-tertiary, var(--hu-accent)) 35%, transparent) 0%,
          transparent 70%
        );
        opacity: 0.7;
        animation: hu-glow-think 2.4s ease-in-out infinite; /* hu-lint-ok: ambient */
      }
    }

    @keyframes hu-glow-speak {
      0%,
      100% {
        opacity: 0.75;
        transform: translate(-50%, -60%) scale(1);
      }
      50% {
        opacity: 1;
        transform: translate(-50%, -60%) scale(1.06);
      }
    }

    @keyframes hu-glow-think {
      0%,
      100% {
        opacity: 0.5;
        transform: translate(-50%, -60%) scale(0.97);
      }
      50% {
        opacity: 0.8;
        transform: translate(-50%, -60%) scale(1.03);
      }
    }

    .mic-btn-wrap {
      position: relative;
      z-index: 1;
    }

    .mic-btn {
      width: var(--hu-space-5xl);
      height: var(--hu-space-5xl);
      border-radius: 50%;
      border: 2px solid var(--hu-border);
      background: var(--hu-bg-surface);
      background-image: var(--hu-surface-gradient);
      color: var(--hu-text-muted);
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      position: relative;
      box-shadow: var(--hu-shadow-card);
      transition:
        background var(--hu-duration-fast) var(--hu-ease-out),
        border-color var(--hu-duration-fast) var(--hu-ease-out),
        box-shadow var(--hu-duration-normal) var(--hu-ease-out),
        transform var(--hu-duration-fast) var(--hu-ease-out);
      & svg {
        width: 2.5rem;
        height: 2.5rem;
        filter: drop-shadow(0 1px 1px color-mix(in srgb, var(--hu-text) 10%, transparent));
      }
      &:hover:not(:disabled) {
        background: var(--hu-hover-overlay);
        border-color: var(--hu-accent);
        color: var(--hu-accent-text, var(--hu-accent));
        box-shadow: var(--hu-shadow-md);
        transform: translateY(-2px);
      }
      &:active:not(:disabled) {
        transform: translateY(1px) scaleY(0.97) scaleX(1.01);
      }
      &:focus-visible {
        outline: 2px solid var(--hu-accent);
        outline-offset: var(--hu-space-xs);
      }
      &:disabled {
        opacity: var(--hu-opacity-disabled, 0.5);
        cursor: not-allowed;
      }
      &.active {
        background: var(--hu-accent);
        background-image: var(--hu-button-gradient-primary);
        border-color: var(--hu-accent);
        color: var(--hu-on-accent);
        box-shadow: var(--hu-shadow-glow-accent);
        transform: translateY(-1px);
      }
      &.speaking {
        border-color: var(--hu-accent-secondary);
        box-shadow: 0 0 0 3px color-mix(in srgb, var(--hu-accent-secondary) 20%, transparent);
      }
      &.processing {
        border-color: var(--hu-accent-tertiary, var(--hu-accent));
        box-shadow: 0 0 0 3px
          color-mix(in srgb, var(--hu-accent-tertiary, var(--hu-accent)) 20%, transparent);
        animation: hu-btn-think-pulse 2.4s ease-in-out infinite; /* hu-lint-ok: ambient */
      }
    }

    @keyframes hu-btn-think-pulse {
      0%,
      100% {
        opacity: 0.7;
      }
      50% {
        opacity: 1;
      }
    }

    .mic-ring {
      position: absolute;
      top: 50%;
      left: 50%;
      width: var(--hu-space-5xl);
      height: var(--hu-space-5xl);
      border-radius: 50%;
      border: 2px solid var(--hu-accent);
      transform: translate(-50%, -50%) scale(1);
      opacity: 0;
      pointer-events: none;
    }

    .mic-ring.active {
      animation: hu-ring-expand calc(1.4s + var(--_level, 0) * -0.6s) ease-out infinite; /* hu-lint-ok: ambient */
    }

    .mic-ring.ring-2.active {
      animation-delay: 0.5s;
    }

    .mic-ring.ring-3.active {
      animation-delay: 1s;
    }

    .mic-ring.processing {
      border-color: var(--hu-accent-tertiary, var(--hu-accent));
      animation: hu-ring-think 2.4s ease-in-out infinite; /* hu-lint-ok: ambient */
    }

    .mic-ring.ring-2.processing {
      animation-delay: 0.8s;
    }

    .mic-ring.ring-3.processing {
      animation-delay: 1.6s;
    }

    @keyframes hu-ring-expand {
      0% {
        transform: translate(-50%, -50%) scale(1);
        opacity: 0.6;
      }
      100% {
        transform: translate(-50%, -50%) scale(calc(1.8 + var(--_level, 0) * 0.8));
        opacity: 0;
      }
    }

    @keyframes hu-ring-think {
      0%,
      100% {
        transform: translate(-50%, -50%) scale(1);
        opacity: 0;
      }
      50% {
        transform: translate(-50%, -50%) scale(1.35);
        opacity: 0.3;
      }
    }

    .voice-status {
      margin-top: var(--hu-space-lg);
      font-size: var(--hu-text-sm);
      color: var(--hu-text-muted);
      position: relative;
      z-index: 1;
    }

    .voice-status.listening,
    .voice-status.processing {
      color: var(--hu-accent-text, var(--hu-accent));
      font-weight: var(--hu-weight-medium);
    }

    .voice-status.speaking {
      color: var(--hu-accent-secondary);
      font-weight: var(--hu-weight-medium);
    }

    @container (max-width: 30rem) /* --hu-breakpoint-sm */ {
      .mic-btn {
        width: 4.5rem;
        height: 4.5rem;
      }
      .mic-btn svg {
        width: 2rem;
        height: 2rem;
      }
    }

    @media (prefers-reduced-motion: reduce) {
      .mic-orb-glow,
      .mic-ring,
      .mic-btn {
        animation: none !important;
        transition: none !important;
      }
      .mic-orb-glow.active {
        opacity: 0.8;
        transform: translate(-50%, -60%) scale(1);
      }
      .mic-btn.active {
        box-shadow: var(--hu-shadow-glow-accent);
      }
    }
  `;

  private _handleClick(): void {
    if (this.disabled) return;
    this.dispatchEvent(
      new CustomEvent("hu-voice-mic-toggle", {
        bubbles: true,
        composed: true,
      }),
    );
  }

  private get _statusText(): string {
    switch (this.state) {
      case "listening":
        return "Listening…";
      case "speaking":
        return "Speaking…";
      case "processing":
        return "Processing…";
      case "unsupported":
        return "Speech recognition not supported in this browser";
      default:
        return "Click the microphone to start speaking";
    }
  }

  private get _buttonLabel(): string {
    switch (this.state) {
      case "listening":
        return "Stop listening";
      case "speaking":
        return "Interrupt and speak";
      case "processing":
        return "Processing voice";
      default:
        return "Start listening";
    }
  }

  override render() {
    const isActive = this.state === "listening";
    const isSpeaking = this.state === "speaking";
    const isProcessing = this.state === "processing";
    const glowClass = isProcessing
      ? "processing"
      : isSpeaking
        ? "speaking"
        : isActive
          ? "active"
          : "";
    const ringClass = isProcessing ? "processing" : isActive ? "active" : "";
    const btnClass = isProcessing
      ? "processing"
      : isSpeaking
        ? "speaking"
        : isActive
          ? "active"
          : "";
    const level = Math.min(1, Math.max(0, this.audioLevel));
    return html`
      <div class="mic-orb-glow ${glowClass}" style="--_level: ${level}"></div>
      <div class="mic-btn-wrap">
        <div class="mic-ring ${ringClass}" style="--_level: ${level}"></div>
        <div class="mic-ring ring-2 ${ringClass}" style="--_level: ${level}"></div>
        <div class="mic-ring ring-3 ${ringClass}" style="--_level: ${level}"></div>
        <button
          class="mic-btn ${btnClass}"
          ?disabled=${this.disabled}
          @click=${this._handleClick}
          aria-label=${this._buttonLabel}
          aria-busy=${isProcessing}
        >
          ${icons.mic}
        </button>
      </div>
      <div class="voice-status ${this.state}" aria-live="polite">${this._statusText}</div>
    `;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "hu-voice-orb": ScVoiceOrb;
  }
}
