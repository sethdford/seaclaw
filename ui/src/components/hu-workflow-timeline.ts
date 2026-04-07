import { LitElement, html, css } from "lit";
import { customElement, property } from "lit/decorators.js";

export type WorkflowEventType = "completed" | "running" | "waiting" | "failed";

export interface WorkflowEvent {
  id: string;
  label: string;
  timestamp: string;
  type: WorkflowEventType;
  details?: string;
}

type WorkflowStatus = "running" | "paused" | "completed" | "failed";

@customElement("hu-workflow-timeline")
export class HuWorkflowTimeline extends LitElement {
  @property({ type: Array }) events: WorkflowEvent[] = [];
  @property() workflowId = "";
  @property() status: WorkflowStatus = "running";

  static override styles = css`
    :host {
      display: block;
      font-family: var(--hu-font);
    }

    .timeline {
      display: flex;
      flex-direction: column;
      gap: var(--hu-space-md);
      position: relative;
      padding-inline-start: var(--hu-space-lg);
    }

    .timeline::before {
      content: "";
      position: absolute;
      left: 11px;
      top: var(--hu-space-lg);
      bottom: 0;
      width: 2px;
      background: var(--hu-border);
    }

    .timeline-event {
      position: relative;
      display: flex;
      flex-direction: column;
      gap: var(--hu-space-sm);
      animation: hu-timeline-slide-in var(--hu-duration-moderate) var(--hu-ease-out) both;
    }

    @keyframes hu-timeline-slide-in {
      from {
        opacity: 0;
        transform: translateY(var(--hu-space-sm));
      }
      to {
        opacity: 1;
        transform: translateY(0);
      }
    }

    .timeline-event:nth-child(1) {
      animation-delay: 0ms;
    }

    .timeline-event:nth-child(2) {
      animation-delay: 50ms;
    }

    .timeline-event:nth-child(3) {
      animation-delay: 100ms;
    }

    .timeline-event:nth-child(4) {
      animation-delay: 150ms;
    }

    .timeline-event:nth-child(5) {
      animation-delay: 200ms;
    }

    .timeline-event:nth-child(n + 6) {
      animation-delay: 250ms;
    }

    .dot {
      position: absolute;
      left: -31px;
      top: var(--hu-space-xs);
      width: 24px;
      height: 24px;
      border-radius: 50%;
      display: flex;
      align-items: center;
      justify-content: center;
      flex-shrink: 0;
      border: 3px solid var(--hu-bg-surface);
      font-weight: 600;
      font-size: var(--hu-text-2xs);
      color: white;
    }

    .dot.completed {
      background: var(--hu-success);
    }

    .dot.running {
      background: var(--hu-accent);
      animation: hu-timeline-pulse var(--hu-duration-slow) var(--hu-ease-in-out) infinite;
    }

    .dot.waiting {
      background: var(--hu-warning);
    }

    .dot.failed {
      background: var(--hu-error);
    }

    @keyframes hu-timeline-pulse {
      0%,
      100% {
        box-shadow: 0 0 0 0 color-mix(in srgb, var(--hu-accent) 60%, transparent);
      }
      50% {
        box-shadow: 0 0 0 6px color-mix(in srgb, var(--hu-accent) 0%, transparent);
      }
    }

    .event-content {
      display: flex;
      flex-direction: column;
      gap: var(--hu-space-2xs);
    }

    .event-label {
      font-size: var(--hu-text-sm);
      font-weight: 600;
      color: var(--hu-text);
      margin: 0;
    }

    .event-timestamp {
      font-size: var(--hu-text-xs);
      color: var(--hu-text-muted);
      margin: 0;
    }

    .event-details {
      font-size: var(--hu-text-sm);
      color: var(--hu-text-secondary);
      padding: var(--hu-space-sm);
      background: color-mix(in srgb, var(--hu-bg-elevated) 50%, var(--hu-bg-surface));
      border-radius: var(--hu-radius-md);
      border-left: 3px solid var(--hu-border);
      margin-top: var(--hu-space-xs);
    }

    .status-info {
      display: flex;
      align-items: center;
      gap: var(--hu-space-sm);
      padding: var(--hu-space-sm) var(--hu-space-md);
      background: color-mix(in srgb, var(--hu-bg-elevated) 50%, var(--hu-bg-surface));
      border-radius: var(--hu-radius-md);
      border: 1px solid var(--hu-border);
      margin-bottom: var(--hu-space-md);
    }

    .status-dot {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      flex-shrink: 0;
    }

    .status-dot.running {
      background: var(--hu-accent);
      animation: hu-timeline-pulse var(--hu-duration-slowest) var(--hu-ease-in-out) infinite;
    }

    .status-dot.paused {
      background: var(--hu-warning);
    }

    .status-dot.completed {
      background: var(--hu-success);
    }

    .status-dot.failed {
      background: var(--hu-error);
    }

    .status-text {
      font-size: var(--hu-text-sm);
      color: var(--hu-text-secondary);
      margin: 0;
    }

    @media (prefers-reduced-motion: reduce) {
      .timeline-event {
        animation: none;
      }
      .dot.running {
        animation: none;
      }
      .status-dot.running {
        animation: none;
      }
    }
  `;

  override render() {
    return html`
      ${this._renderStatusInfo()}
      <div class="timeline" role="list">
        ${this.events.map(
          (event) => html`
            <div class="timeline-event" role="listitem">
              <div class="dot ${event.type}"></div>
              <div class="event-content">
                <p class="event-label">${event.label}</p>
                <p class="event-timestamp">${this._formatTimestamp(event.timestamp)}</p>
                ${event.details ? html`<div class="event-details">${event.details}</div>` : ""}
              </div>
            </div>
          `,
        )}
      </div>
    `;
  }

  private _renderStatusInfo() {
    const statusLabels: Record<WorkflowStatus, string> = {
      running: "Workflow running",
      paused: "Workflow paused",
      completed: "Workflow completed",
      failed: "Workflow failed",
    };

    return html`
      <div class="status-info" role="status" aria-live="polite">
        <div class="status-dot ${this.status}"></div>
        <p class="status-text">${statusLabels[this.status]}</p>
      </div>
    `;
  }

  private _formatTimestamp(timestamp: string): string {
    try {
      const date = new Date(timestamp);
      const now = new Date();
      const diff = now.getTime() - date.getTime();

      // Less than 1 minute
      if (diff < 60000) {
        return "Just now";
      }

      // Less than 1 hour
      if (diff < 3600000) {
        const minutes = Math.floor(diff / 60000);
        return `${minutes}m ago`;
      }

      // Less than 1 day
      if (diff < 86400000) {
        const hours = Math.floor(diff / 3600000);
        return `${hours}h ago`;
      }

      // Fallback: formatted date
      return date.toLocaleDateString("en-US", {
        month: "short",
        day: "numeric",
        hour: "2-digit",
        minute: "2-digit",
      });
    } catch {
      return timestamp;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "hu-workflow-timeline": HuWorkflowTimeline;
  }
}
