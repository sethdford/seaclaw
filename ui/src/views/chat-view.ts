import { html, css, nothing } from "lit";
import { customElement, property, state, query } from "lit/decorators.js";
import type { ContextMenuItem } from "../components/sc-context-menu.js";
import type { GatewayStatus } from "../gateway.js";
import { GatewayClient as GatewayClientClass } from "../gateway.js";
import { GatewayAwareLitElement } from "../gateway-aware.js";
import { formatTime } from "../utils.js";
import { icons } from "../icons.js";
import { ScToast } from "../components/sc-toast.js";
import { ChatController, type ChatItem, type GatewayLike } from "../controllers/chat-controller.js";
import "../components/sc-composer.js";
import "../components/sc-thinking.js";
import "../components/sc-tool-result.js";
import "../components/sc-message-stream.js";
import "../components/sc-reasoning-block.js";
import "../components/sc-chat-search.js";
import "../components/sc-context-menu.js";
import "../components/sc-skeleton.js";

@customElement("sc-chat-view")
export class ScChatView extends GatewayAwareLitElement {
  static override styles = css`
    :host {
      display: flex;
      flex-direction: column;
      height: 100%;
      max-height: calc(100vh - 120px);
    }
    .container {
      display: flex;
      flex-direction: column;
      height: 100%;
      max-width: 720px;
      margin: 0 auto;
      position: relative;
      width: 100%;
    }
    .status-bar {
      display: flex;
      align-items: center;
      gap: var(--sc-space-sm);
      padding: var(--sc-space-sm) var(--sc-space-md);
      font-size: var(--sc-text-xs);
      color: var(--sc-text-muted);
      background: var(--sc-bg-surface);
      border-bottom: 1px solid var(--sc-border);
    }
    .status-dot {
      width: 8px;
      height: 8px;
      border-radius: 50%;
    }
    .status-dot.connected {
      background: var(--sc-success);
    }
    .status-dot.connecting {
      background: var(--sc-warning);
      animation: sc-pulse var(--sc-duration-slow) var(--sc-ease-in-out) infinite;
    }
    .status-dot.disconnected {
      background: var(--sc-text-muted);
    }
    .messages {
      flex: 1;
      overflow-y: auto;
      padding: var(--sc-space-md);
      display: flex;
      flex-direction: column;
      gap: var(--sc-space-md);
    }
    .message {
      max-width: 85%;
      padding: var(--sc-space-md) var(--sc-space-md);
      border-radius: var(--sc-radius);
      font-size: var(--sc-text-base);
      line-height: 1.5;
      display: flex;
      flex-direction: column;
      gap: var(--sc-space-xs);
      animation: sc-slide-up var(--sc-duration-normal)
        var(--sc-ease-spring, cubic-bezier(0.34, 1.56, 0.64, 1)) both;
    }
    .message.user {
      align-self: flex-end;
      background: var(--sc-accent);
      color: var(--sc-bg);
    }
    .message.assistant {
      align-self: flex-start;
      background: var(--sc-bg-surface);
      border: 1px solid var(--sc-border);
      color: var(--sc-text);
    }
    .message-meta {
      font-size: var(--sc-text-xs);
      opacity: var(--sc-opacity-muted, 0.8);
    }
    .message.user .message-meta {
      align-self: flex-end;
    }
    .message.assistant .message-meta {
      align-self: flex-start;
    }
    .message code {
      font-family: var(--sc-font-mono);
      font-size: var(--sc-text-sm);
      background: var(--sc-bg-elevated);
      padding: var(--sc-space-2xs) var(--sc-space-xs);
      border-radius: var(--sc-radius-sm);
    }
    .scroll-bottom-pill {
      position: absolute;
      bottom: 90px;
      left: 50%;
      transform: translateX(-50%);
      background: var(--sc-bg-surface);
      border: 1px solid var(--sc-border);
      box-shadow: var(--sc-shadow-md);
      padding: var(--sc-space-xs) var(--sc-space-md);
      border-radius: var(--sc-radius-full);
      font-size: var(--sc-text-xs);
      font-family: var(--sc-font);
      color: var(--sc-text);
      cursor: pointer;
      display: flex;
      align-items: center;
      gap: var(--sc-space-xs);
      z-index: 5;
      animation: sc-fade-up var(--sc-duration-fast) var(--sc-ease-out);
    }
    .scroll-bottom-pill:hover {
      background: var(--sc-bg-elevated);
    }
    .pill-icon svg {
      width: 14px;
      height: 14px;
      vertical-align: -2px;
    }
    @keyframes sc-fade-up {
      from {
        opacity: 0;
        transform: translateX(-50%) translateY(8px);
      }
      to {
        opacity: 1;
        transform: translateX(-50%) translateY(0);
      }
    }
    .retry-btn {
      background: transparent;
      border: 1px solid var(--sc-border);
      color: var(--sc-text-muted);
      font-size: var(--sc-text-xs);
      font-family: var(--sc-font);
      padding: var(--sc-space-2xs) var(--sc-space-sm);
      border-radius: var(--sc-radius-sm);
      cursor: pointer;
      margin-top: var(--sc-space-xs);
      transition:
        color var(--sc-duration-fast),
        border-color var(--sc-duration-fast);
    }
    .retry-btn:hover {
      color: var(--sc-text);
      border-color: var(--sc-text-muted);
    }
    .message a {
      color: var(--sc-accent-text, var(--sc-accent));
      text-decoration: underline;
    }
    .message a:hover {
      color: var(--sc-accent-hover);
    }
    .thinking {
      display: flex;
      align-items: center;
      gap: var(--sc-space-sm);
      align-self: flex-start;
      padding: var(--sc-space-sm) var(--sc-space-md);
      font-size: var(--sc-text-base);
      color: var(--sc-text-muted);
      font-style: italic;
    }
    .stream-elapsed {
      font-size: var(--sc-text-xs);
      color: var(--sc-text-muted);
      font-variant-numeric: tabular-nums;
    }
    .abort-btn {
      margin-left: var(--sc-space-md);
      padding: var(--sc-space-xs) var(--sc-space-md);
      background: var(--sc-error-dim);
      color: var(--sc-error);
      border: 1px solid var(--sc-error);
      border-radius: var(--sc-radius);
      cursor: pointer;
      font-size: var(--sc-text-xs);
      font-style: normal;
    }
    .abort-btn:hover {
      background: var(--sc-error-dim);
      border-color: var(--sc-error);
    }
    .typing-dots {
      display: inline-flex;
      gap: var(--sc-space-xs);
      align-items: center;
      margin-left: var(--sc-space-xs);
    }
    .typing-dots span {
      width: 6px;
      height: 6px;
      border-radius: 50%;
      background: var(--sc-text-muted);
      animation: sc-bounce-dot var(--sc-duration-slow) var(--sc-ease-in-out) infinite both;
    }
    .typing-dots span:nth-child(1) {
      animation-delay: -0.32s;
    }
    .typing-dots span:nth-child(2) {
      animation-delay: -0.16s;
    }
    @keyframes sc-bounce-dot {
      0%,
      80%,
      100% {
        transform: scale(0);
      }
      40% {
        transform: scale(1);
      }
    }
    .error-banner {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: var(--sc-space-md) var(--sc-space-md);
      background: var(--sc-error-dim);
      border: 1px solid var(--sc-error);
      border-radius: var(--sc-radius);
      color: var(--sc-error);
      font-size: var(--sc-text-base);
    }
    .error-banner button {
      background: none;
      border: none;
      color: inherit;
      cursor: pointer;
      display: flex;
      align-items: center;
      padding: 4px;
    }
    .error-banner button svg {
      width: 16px;
      height: 16px;
      line-height: 1;
    }
    @media (max-width: 640px) {
      .message {
        max-width: 95%;
      }
    }
    .history-skeleton {
      display: flex;
      flex-direction: column;
      gap: var(--sc-space-md);
      padding: var(--sc-space-lg) 0;
    }
    @media (prefers-reduced-motion: reduce) {
      .status-dot.connecting,
      .message,
      .scroll-bottom-pill,
      .typing-dots span {
        animation: none !important;
      }
    }
  `;

  @property() sessionKey = "default";

  private chat = new ChatController(this, () => this.gateway as GatewayLike | null);

  @state() private inputValue = "";
  @state() private connectionStatus: GatewayStatus = "disconnected";
  @state() private showScrollPill = false;
  @state() private _searchOpen = false;
  @state() private _searchQuery = "";
  @state() private _searchCurrentMatch = 0;
  @state() private _contextMenu: {
    open: boolean;
    x: number;
    y: number;
    items: ContextMenuItem[];
  } = { open: false, x: 0, y: 0, items: [] };
  @query("#message-list") private messageList!: HTMLElement;

  private messageHandler = (e: Event) => this.onGatewayMessage(e);
  private statusHandler = (e: Event) => {
    this.connectionStatus = (e as CustomEvent<GatewayStatus>).detail;
  };

  private _scrollHandler = () => {
    const el = this.messageList;
    if (!el) return;
    const atBottom = el.scrollHeight - el.scrollTop - el.clientHeight < 60;
    if (this.showScrollPill === atBottom) this.showScrollPill = !atBottom;
  };

  private _handleKeyDown = (e: KeyboardEvent): void => {
    if ((e.metaKey || e.ctrlKey) && e.key === "f") {
      e.preventDefault();
      this._searchOpen = !this._searchOpen;
    }
  };

  private _getSearchMatchIndices(): number[] {
    const q = this._searchQuery.trim().toLowerCase();
    if (!q) return [];
    const indices: number[] = [];
    this.chat.items.forEach((item, idx) => {
      if (item.type === "message" && item.content.toLowerCase().includes(q)) {
        indices.push(idx);
      }
    });
    return indices;
  }

  private _scrollToMatch(matchIndex: number): void {
    const indices = this._getSearchMatchIndices();
    if (matchIndex < 0 || matchIndex >= indices.length) return;
    const itemIdx = indices[matchIndex];
    this.updateComplete.then(() => {
      const msgEl = this.messageList?.querySelector(`#msg-${itemIdx}`) as HTMLElement;
      msgEl?.scrollIntoView({ block: "nearest", behavior: "smooth" });
    });
  }

  private _handleFiles(files: File[]): void {
    for (const file of files) {
      this.chat.items = [
        ...this.chat.items,
        {
          type: "message",
          role: "user",
          content: `[Attached file: ${file.name} (${(file.size / 1024).toFixed(1)} KB)]`,
          ts: Date.now(),
        },
      ];
    }
    this.chat.cacheMessages(this.sessionKey);
    this.requestUpdate();
    this.scrollToBottom();
  }

  override firstUpdated(): void {
    const gw = this.gateway;
    if (gw) {
      this.connectionStatus = gw.status;
      gw.addEventListener(GatewayClientClass.EVENT_GATEWAY, this.messageHandler);
      gw.addEventListener(GatewayClientClass.EVENT_STATUS, this.statusHandler as EventListener);
    }
    this.messageList?.addEventListener("scroll", this._scrollHandler, { passive: true });
    document.addEventListener("keydown", this._handleKeyDown);
  }

  protected override async load(): Promise<void> {
    await this.chat.loadHistory(this.sessionKey);
  }

  private async handleAbort(): Promise<void> {
    await this.chat.abort();
  }

  override disconnectedCallback(): void {
    document.removeEventListener("keydown", this._handleKeyDown);
    const gw = this.gateway;
    gw?.removeEventListener(GatewayClientClass.EVENT_GATEWAY, this.messageHandler);
    gw?.removeEventListener(GatewayClientClass.EVENT_STATUS, this.statusHandler as EventListener);
    this.messageList?.removeEventListener("scroll", this._scrollHandler);
    super.disconnectedCallback();
  }

  private onGatewayMessage(e: Event): void {
    const ev = e as CustomEvent;
    const detail = ev.detail as { event?: string; payload?: Record<string, unknown> };
    if (!detail?.event) return;
    const payload = detail.payload ?? {};

    if (detail.event === "health") {
      this.requestUpdate();
      return;
    }

    this.chat.handleEvent(detail.event, payload, this.sessionKey);
    this.scrollToBottom();
  }

  private _findLastAssistantIdx(): number {
    for (let i = this.chat.items.length - 1; i >= 0; i--) {
      if (
        this.chat.items[i].type === "message" &&
        (this.chat.items[i] as { role: string }).role === "assistant"
      ) {
        return i;
      }
    }
    return -1;
  }

  private scrollToBottom(): void {
    this.updateComplete.then(() => {
      const el = this.messageList;
      if (el) el.scrollTop = el.scrollHeight;
    });
  }

  private async _retry(): Promise<void> {
    try {
      await this.chat.retry(this.sessionKey);
    } catch (err) {
      const msg = err instanceof Error ? err.message : "Failed to send message";
      ScToast.show({ message: msg, variant: "error" });
    }
    this.scrollToBottom();
  }

  private _copyMessage(item: Extract<ChatItem, { type: "message" }>): void {
    navigator.clipboard?.writeText(item.content).catch(() => {});
    ScToast.show({ message: "Copied to clipboard", variant: "success" });
  }

  private _retryMessage(item: Extract<ChatItem, { type: "message" }>): void {
    if (item.role !== "user") return;
    this._handleSend(item.content);
  }

  private _onMessageContextMenu(e: MouseEvent, item: ChatItem): void {
    e.preventDefault();
    if (item.type !== "message") return;
    this._contextMenu = {
      open: true,
      x: e.clientX,
      y: e.clientY,
      items: [
        {
          label: "Copy message",
          icon: icons.copy,
          action: () => this._copyMessage(item),
        },
        {
          label: "Retry",
          icon: icons["arrow-clockwise"],
          action: () => this._retryMessage(item),
          disabled: item.role === "assistant",
        },
      ],
    };
  }

  private async _handleSend(message: string): Promise<void> {
    if (!message || !this.gateway) return;
    this.inputValue = "";
    try {
      await this.chat.send(message, this.sessionKey);
    } catch (err) {
      const msg = err instanceof Error ? err.message : "Failed to send message";
      ScToast.show({ message: msg, variant: "error" });
    }
    this.scrollToBottom();
  }

  override render() {
    return html`
      <div class="container">
        ${this._renderStatusBar()} ${this._renderErrorBanner()}
        <div
          id="message-list"
          class="messages"
          role="log"
          aria-live="polite"
          aria-label="Chat messages"
        >
          ${this._renderSearch()} ${this._renderMessages()}
          ${this.chat.isWaiting ? this._renderThinking() : nothing}
        </div>
        ${this._renderScrollPill()} ${this._renderRetryButton()}
        <sc-composer
          .value=${this.inputValue}
          .waiting=${this.chat.isWaiting}
          .disabled=${this.connectionStatus === "disconnected"}
          .showSuggestions=${this.chat.items.length === 0}
          .streamElapsed=${this.chat.streamElapsed}
          .placeholder=${this.connectionStatus === "disconnected"
            ? "Disconnected — reconnect to send messages"
            : "Type a message... (Enter to send, Shift+Enter for newline)"}
          @sc-send=${(e: CustomEvent<{ message: string }>) => this._handleSend(e.detail.message)}
          @sc-files=${(e: CustomEvent<{ files: File[] }>) => this._handleFiles(e.detail.files)}
          @sc-use-suggestion=${(e: CustomEvent<{ text: string }>) =>
            this._handleSend(e.detail.text)}
          @sc-input-change=${(e: CustomEvent<{ value: string }>) => {
            this.inputValue = e.detail.value;
          }}
        ></sc-composer>
        ${this._contextMenu.open
          ? html`
              <sc-context-menu
                .open=${this._contextMenu.open}
                .x=${this._contextMenu.x}
                .y=${this._contextMenu.y}
                .items=${this._contextMenu.items}
                @close=${() => (this._contextMenu = { ...this._contextMenu, open: false })}
              ></sc-context-menu>
            `
          : nothing}
      </div>
    `;
  }

  private _renderStatusBar() {
    const label =
      this.connectionStatus === "connected"
        ? "Connected"
        : this.connectionStatus === "connecting"
          ? "Reconnecting\u2026"
          : "Disconnected";
    return html`
      <div class="status-bar">
        <span class="status-dot ${this.connectionStatus}" aria-hidden="true"></span>
        <span>${label}</span>
      </div>
    `;
  }

  private _renderErrorBanner() {
    if (!this.chat.errorBanner) return nothing;
    return html`
      <div class="error-banner">
        <span>${this.chat.errorBanner}</span>
        <button
          class="dismiss-btn"
          @click=${() => (this.chat.errorBanner = "")}
          aria-label="Dismiss"
        >
          ${icons.x}
        </button>
      </div>
    `;
  }

  private _renderSearch() {
    if (!this._searchOpen) return nothing;
    return html`
      <sc-chat-search
        .open=${this._searchOpen}
        .query=${this._searchQuery}
        .matchCount=${this._getSearchMatchIndices().length}
        .currentMatch=${this._searchCurrentMatch}
        @sc-search-change=${(e: CustomEvent<{ query: string }>) => {
          this._searchQuery = e.detail.query;
          this._searchCurrentMatch = 0;
        }}
        @sc-search-next=${() => {
          const indices = this._getSearchMatchIndices();
          if (indices.length === 0) return;
          this._searchCurrentMatch = (this._searchCurrentMatch % indices.length) + 1;
          this._scrollToMatch(this._searchCurrentMatch - 1);
        }}
        @sc-search-prev=${() => {
          const indices = this._getSearchMatchIndices();
          if (indices.length === 0) return;
          this._searchCurrentMatch =
            this._searchCurrentMatch <= 1 ? indices.length : this._searchCurrentMatch - 1;
          this._scrollToMatch(this._searchCurrentMatch - 1);
        }}
        @sc-search-close=${() => {
          this._searchOpen = false;
          this._searchQuery = "";
          this._searchCurrentMatch = 0;
        }}
      ></sc-chat-search>
    `;
  }

  private _renderMessages() {
    if (this.chat.historyLoading) {
      return html`
        <div class="history-skeleton">
          <sc-skeleton variant="line" width="60%"></sc-skeleton>
          <sc-skeleton variant="line" width="80%"></sc-skeleton>
          <sc-skeleton variant="line" width="45%"></sc-skeleton>
        </div>
      `;
    }
    const lastAssistantIdx = this._findLastAssistantIdx();
    return this.chat.items.map((item, idx) => {
      if (item.type === "message") {
        const isStreaming =
          this.chat.isWaiting && item.role === "assistant" && idx === lastAssistantIdx;
        return html`
          <div
            id="msg-${idx}"
            class="message ${item.role}"
            style="--sc-stagger-index: ${idx}; animation-delay: min(calc(var(--sc-stagger-delay) * var(--sc-stagger-index)), var(--sc-stagger-max));"
            @contextmenu=${(ev: MouseEvent) => this._onMessageContextMenu(ev, item)}
          >
            <sc-message-stream
              .content=${item.content}
              .streaming=${isStreaming}
              .role=${item.role}
            ></sc-message-stream>
            ${item.ts != null
              ? html`<span class="message-meta">${formatTime(item.ts)}</span>`
              : nothing}
          </div>
        `;
      }
      if (item.type === "tool_call") {
        return html`
          <sc-tool-result
            .tool=${item.name}
            .status=${item.status === "completed"
              ? item.result?.startsWith("Error")
                ? "error"
                : "success"
              : "running"}
            .content=${item.result ?? item.input ?? ""}
          ></sc-tool-result>
        `;
      }
      if (item.type === "thinking") {
        return html`
          <sc-reasoning-block
            .content=${item.content}
            .streaming=${item.streaming}
            .duration=${item.duration ?? ""}
          ></sc-reasoning-block>
        `;
      }
      return nothing;
    });
  }

  private _renderThinking() {
    return html`
      <div class="thinking">
        <sc-thinking .active=${true} .steps=${[]}></sc-thinking>
        <span class="stream-elapsed">${this.chat.streamElapsed}</span>
        <button class="abort-btn" @click=${() => this.handleAbort()} aria-label="Stop generating">
          Abort
        </button>
      </div>
    `;
  }

  private _renderScrollPill() {
    if (!this.showScrollPill) return nothing;
    return html`
      <button
        class="scroll-bottom-pill"
        @click=${() => this.scrollToBottom()}
        aria-label="Scroll to latest messages"
      >
        <span class="pill-icon">${icons["arrow-down"]}</span> New messages
      </button>
    `;
  }

  private _renderRetryButton() {
    if (!this.chat.lastFailedMessage) return nothing;
    return html`<button class="retry-btn" @click=${this._retry} aria-label="Retry last message">
      Retry last message
    </button>`;
  }
}
