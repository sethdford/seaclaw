import { LitElement, html, css, nothing } from "lit";
import { customElement, state } from "lit/decorators.js";
import type { GatewayClient } from "../gateway.js";
import { GatewayClient as GatewayClientClass } from "../gateway.js";
import { getGateway } from "../gateway-provider.js";

type VoiceStatus = "idle" | "listening" | "processing" | "unsupported";

@customElement("sc-voice-view")
export class ScVoiceView extends LitElement {
  static override styles = css`
    :host {
      display: block;
      color: var(--sc-text);
      max-width: 480px;
      margin: 0 auto;
    }
    .header {
      margin-bottom: 1.5rem;
    }
    h2 {
      margin: 0;
      font-size: 1.25rem;
      font-weight: 600;
      color: var(--sc-text);
    }
    .mic-area {
      display: flex;
      flex-direction: column;
      align-items: center;
      margin-bottom: 1.5rem;
    }
    .mic-btn {
      width: 80px;
      height: 80px;
      border-radius: 50%;
      border: none;
      background: var(--sc-bg-surface);
      border: 2px solid var(--sc-border);
      color: var(--sc-text-muted);
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      font-size: 2rem;
      transition:
        background 0.2s,
        border-color 0.2s,
        transform 0.2s;
    }
    .mic-btn:hover {
      background: var(--sc-bg-elevated);
      border-color: var(--sc-accent);
      color: var(--sc-accent);
    }
    .mic-btn.active {
      background: var(--sc-accent);
      border-color: var(--sc-accent);
      color: var(--sc-bg);
      animation: pulse-mic 1.5s ease-in-out infinite;
    }
    @keyframes pulse-mic {
      0%,
      100% {
        transform: scale(1);
        box-shadow: 0 0 0 0 rgba(249, 112, 102, 0.4);
      }
      50% {
        transform: scale(1.05);
        box-shadow: 0 0 0 12px rgba(249, 112, 102, 0);
      }
    }
    .transcript-area {
      margin-bottom: 1rem;
    }
    .transcript-area textarea {
      width: 100%;
      min-height: 80px;
      padding: 0.75rem 1rem;
      background: var(--sc-bg-surface);
      border: 1px solid var(--sc-border);
      border-radius: var(--sc-radius);
      color: var(--sc-text);
      font-family: var(--sc-font);
      font-size: 0.875rem;
      resize: vertical;
      box-sizing: border-box;
    }
    .transcript-area textarea:focus {
      outline: none;
      border-color: var(--sc-accent);
    }
    .transcript-area textarea::placeholder {
      color: var(--sc-text-muted);
    }
    .send-row {
      display: flex;
      gap: 0.5rem;
      margin-bottom: 1.5rem;
    }
    .send-btn {
      padding: 0.5rem 1rem;
      background: var(--sc-accent);
      color: var(--sc-bg);
      border: none;
      border-radius: var(--sc-radius);
      font-weight: 500;
      cursor: pointer;
      font-size: 0.875rem;
    }
    .send-btn:hover:not(:disabled) {
      background: var(--sc-accent-hover);
    }
    .send-btn:disabled {
      opacity: 0.5;
      cursor: not-allowed;
    }
    .response-area {
      padding: 1rem;
      background: var(--sc-bg-surface);
      border: 1px solid var(--sc-border);
      border-radius: var(--sc-radius);
      min-height: 80px;
      font-size: 0.875rem;
      line-height: 1.5;
      white-space: pre-wrap;
      word-break: break-word;
    }
    .response-area.empty {
      color: var(--sc-text-muted);
    }
    .status-line {
      margin-top: 1rem;
      font-size: 0.8125rem;
      color: var(--sc-text-muted);
    }
    .status-line.listening {
      color: var(--sc-accent);
    }
    .status-line.processing {
      color: var(--sc-accent);
    }
  `;

  @state() private transcript = "";
  @state() private response = "";
  @state() private voiceStatus: VoiceStatus = "idle";
  @state() private speechSupported = true;
  @state() private recognition: SpeechRecognition | null = null;

  private get gateway(): GatewayClient | null {
    return getGateway();
  }

  private gatewayHandler = (e: Event): void => this.onGatewayEvent(e);
  private _boundGateway: GatewayClient | null = null;

  override connectedCallback(): void {
    super.connectedCallback();
    this.speechSupported =
      "webkitSpeechRecognition" in window || "SpeechRecognition" in window;
    if (!this.speechSupported) this.voiceStatus = "unsupported";
    this._boundGateway = this.gateway;
    if (this._boundGateway) {
      this._boundGateway.addEventListener(
        GatewayClientClass.EVENT_GATEWAY,
        this.gatewayHandler,
      );
    }
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this._boundGateway?.removeEventListener(
      GatewayClientClass.EVENT_GATEWAY,
      this.gatewayHandler,
    );
    this._boundGateway = null;
    this.stopRecognition();
  }

  private onGatewayEvent(e: Event): void {
    const ev = e as CustomEvent<{
      event: string;
      payload?: Record<string, unknown>;
    }>;
    const detail = ev.detail;
    if (!detail?.event || detail.event !== "chat") return;
    const payload = detail.payload ?? {};
    const sessionKey =
      (payload.session_key as string) ?? (payload.sessionKey as string);
    if (sessionKey !== "voice") return;
    const state = payload.state as string;
    const content = (payload.message as string) ?? "";
    if (state === "sent" && content) {
      this.response = content;
      this.voiceStatus = "idle";
    }
    if (state === "chunk" && content) {
      this.response += content;
      this.voiceStatus = "processing";
    }
    if (state === "received") {
      this.voiceStatus = "processing";
    }
    this.requestUpdate();
  }

  private startRecognition(): void {
    if (!this.speechSupported) return;
    const Ctor =
      (
        window as unknown as {
          webkitSpeechRecognition?: new () => SpeechRecognition;
        }
      ).webkitSpeechRecognition ??
      (window as unknown as { SpeechRecognition?: new () => SpeechRecognition })
        .SpeechRecognition;
    if (!Ctor) return;
    const rec = new Ctor();
    rec.continuous = false;
    rec.interimResults = true;
    rec.lang = "en-US";
    rec.onresult = (event: SpeechRecognitionEvent) => {
      const r = event.results[event.results.length - 1];
      if (!r) return;
      this.transcript = r[0]?.transcript ?? "";
      this.requestUpdate();
    };
    rec.onend = () => {
      if (this.voiceStatus === "listening") this.voiceStatus = "idle";
      this.recognition = null;
      this.requestUpdate();
    };
    rec.onerror = () => {
      this.voiceStatus = "idle";
      this.recognition = null;
      this.requestUpdate();
    };
    rec.start();
    this.recognition = rec;
    this.voiceStatus = "listening";
  }

  private stopRecognition(): void {
    if (this.recognition) {
      try {
        this.recognition.stop();
      } catch {
        /* ignore */
      }
      this.recognition = null;
    }
    if (this.voiceStatus === "listening") this.voiceStatus = "idle";
  }

  private toggleMic(): void {
    if (this.voiceStatus === "listening") {
      this.stopRecognition();
    } else {
      this.startRecognition();
    }
  }

  private async send(): Promise<void> {
    const text = this.transcript.trim();
    const gw = this.gateway;
    if (!text || !gw) return;
    this.voiceStatus = "processing";
    try {
      await gw.request<{ status?: string; sessionKey?: string }>("chat.send", {
        message: text,
        sessionKey: "voice",
      });
    } catch {
      this.voiceStatus = "idle";
    }
  }

  private get statusText(): string {
    switch (this.voiceStatus) {
      case "listening":
        return "Listening...";
      case "processing":
        return "Processing...";
      case "unsupported":
        return "Speech not supported";
      default:
        return "Click mic to start";
    }
  }

  override render() {
    return html`
      <div class="header">
        <h2>Voice & Speech</h2>
      </div>

      <div class="mic-area">
        <button
          class="mic-btn ${this.voiceStatus === "listening" ? "active" : ""}"
          ?disabled=${!this.speechSupported}
          @click=${this.toggleMic}
          aria-label=${this.voiceStatus === "listening"
            ? "Stop listening"
            : "Start listening"}
        >
          🎤
        </button>
      </div>

      <div class="transcript-area">
        <textarea
          placeholder="Recognized speech will appear here..."
          .value=${this.transcript}
          @input=${(e: Event) => {
            this.transcript = (e.target as HTMLTextAreaElement).value;
          }}
        ></textarea>
      </div>

      <div class="send-row">
        <button
          class="send-btn"
          ?disabled=${!this.transcript.trim() ||
          this.voiceStatus === "processing"}
          @click=${() => this.send()}
        >
          Send
        </button>
      </div>

      <div class="response-area ${this.response ? "" : "empty"}">
        ${this.response || "Assistant response will appear here."}
      </div>

      <div class="status-line ${this.voiceStatus}">${this.statusText}</div>
    `;
  }
}
