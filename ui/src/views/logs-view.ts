import { LitElement, html, css } from "lit";
import { customElement, state } from "lit/decorators.js";
import { GatewayClient } from "../gateway.js";
import { getGateway } from "../gateway-provider.js";
import { EVENT_NAMES } from "../utils.js";

interface LogEntry {
  ts: string;
  event: string;
  payload: Record<string, unknown>;
}

@customElement("sc-logs-view")
export class ScLogsView extends LitElement {
  static override styles = css`
    :host {
      display: block;
      color: var(--sc-text);
    }
    .header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-bottom: 1rem;
      flex-wrap: wrap;
      gap: 0.5rem;
    }
    h2 {
      margin: 0;
      font-size: 1.25rem;
      font-weight: 600;
      color: var(--sc-text);
    }
    .controls {
      display: flex;
      gap: 0.5rem;
      align-items: center;
    }
    .filter-input {
      padding: 0.5rem 0.75rem;
      background: var(--sc-bg);
      border: 1px solid var(--sc-border);
      border-radius: var(--sc-radius);
      color: var(--sc-text);
      font-size: 0.875rem;
      width: 200px;
    }
    .btn {
      padding: 0.5rem 1rem;
      background: var(--sc-bg-elevated);
      color: var(--sc-text);
      border: 1px solid var(--sc-border);
      border-radius: var(--sc-radius);
      cursor: pointer;
      font-size: 0.875rem;
    }
    .btn:hover {
      background: var(--sc-border);
    }
    .log-area {
      background: var(--sc-bg-surface);
      border: 1px solid var(--sc-border);
      border-radius: var(--sc-radius);
      padding: 1rem;
      height: 400px;
      overflow-y: auto;
      font-family: var(--sc-font-mono);
      font-size: 0.75rem;
      line-height: 1.5;
      color: var(--sc-text);
    }
    .log-line {
      margin-bottom: 0.25rem;
      word-break: break-all;
    }
    .log-ts {
      color: var(--sc-text-muted);
      margin-right: 0.5rem;
    }
    .event {
      font-weight: 600;
      margin-right: 0.5rem;
    }
  `;

  @state() private logs: LogEntry[] = [];
  @state() private filter = "";
  @state() private logAreaRef: HTMLDivElement | null = null;

  private get gateway(): GatewayClient | null {
    return getGateway();
  }

  override connectedCallback(): void {
    super.connectedCallback();
    this.setupListener();
  }

  override disconnectedCallback(): void {
    this.removeListener();
    super.disconnectedCallback();
  }

  private setupListener(): void {
    const gw = this.gateway;
    if (!gw) return;
    gw.addEventListener(
      GatewayClient.EVENT_GATEWAY,
      this.handleGatewayEvent as EventListener,
    );
  }

  private removeListener(): void {
    const gw = this.gateway;
    if (!gw) return;
    gw.removeEventListener(
      GatewayClient.EVENT_GATEWAY,
      this.handleGatewayEvent as EventListener,
    );
  }

  private handleGatewayEvent = (e: Event): void => {
    const ev = (
      e as CustomEvent<{ event: string; payload: Record<string, unknown> }>
    ).detail;
    this.logs = [
      ...this.logs,
      {
        ts: new Date().toISOString(),
        event: ev.event,
        payload: ev.payload ?? {},
      },
    ];
    this.requestUpdate();
    this.scrollToBottom();
  };

  private eventColor(event: string): string {
    switch (event) {
      case EVENT_NAMES.CHAT:
        return "var(--sc-success)";
      case EVENT_NAMES.TOOL_CALL:
        return "var(--sc-info)";
      case EVENT_NAMES.ERROR:
        return "var(--sc-error)";
      case EVENT_NAMES.HEALTH:
        return "var(--sc-warning)";
      default:
        return "var(--sc-text)";
    }
  }

  private scrollToBottom(): void {
    this.updateComplete.then(() => {
      const el = this.shadowRoot?.querySelector(".log-area");
      if (el) el.scrollTop = el.scrollHeight;
    });
  }

  private clearLogs(): void {
    this.logs = [];
  }

  private get filteredLogs(): LogEntry[] {
    if (!this.filter.trim()) return this.logs;
    const q = this.filter.toLowerCase();
    return this.logs.filter(
      (l) =>
        l.event.toLowerCase().includes(q) ||
        JSON.stringify(l.payload).toLowerCase().includes(q),
    );
  }

  override render() {
    const entries = this.filteredLogs;

    return html`
      <div class="header">
        <h2>Logs</h2>
        <div class="controls">
          <input
            type="text"
            class="filter-input"
            placeholder="Filter..."
            .value=${this.filter}
            @input=${(e: Event) =>
              (this.filter = (e.target as HTMLInputElement).value)}
          />
          <button class="btn" @click=${this.clearLogs}>Clear</button>
        </div>
      </div>
      <div class="log-area" role="log">
        ${entries.length === 0
          ? html`<span style="color: var(--sc-text-muted)"
              >Listening for gateway events...</span
            >`
          : entries.map(
              (l) => html`
                <div class="log-line">
                  <span class="log-ts">${l.ts}</span>
                  <span class="event" style="color: ${this.eventColor(l.event)}"
                    >[${l.event}]</span
                  >
                  ${JSON.stringify(l.payload)}
                </div>
              `,
            )}
      </div>
    `;
  }
}
