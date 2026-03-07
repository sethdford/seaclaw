import { LitElement, html, css, nothing } from "lit";
import { customElement, property } from "lit/decorators.js";

export interface TimelineItem {
  time: string;
  message: string;
  status: "success" | "error" | "info" | "pending";
  detail?: string;
}

@customElement("sc-timeline")
export class ScTimeline extends LitElement {
  @property({ type: Array }) items: TimelineItem[] = [];
  @property({ type: Number }) max = 10;

  static override styles = css`
    :host {
      display: block;
    }
    .timeline {
      display: flex;
      flex-direction: column;
      position: relative;
    }
    .entry {
      display: grid;
      grid-template-columns: 12px 1fr;
      gap: var(--sc-space-md);
      padding-bottom: var(--sc-space-md);
      position: relative;
      animation: sc-slide-up 300ms var(--sc-ease-out) both;
    }
    .dot {
      width: 8px;
      height: 8px;
      border-radius: var(--sc-radius-full);
      margin-top: 6px;
      flex-shrink: 0;
      justify-self: center;
    }
    .dot.success {
      background: var(--sc-success);
    }
    .dot.error {
      background: var(--sc-error);
    }
    .dot.info {
      background: var(--sc-accent);
    }
    .dot.pending {
      background: var(--sc-text-muted);
    }
    .line {
      position: absolute;
      left: 5px;
      top: 16px;
      bottom: 0;
      width: 1px;
      background: var(--sc-border-subtle);
    }
    .entry:last-child .line {
      display: none;
    }
    .time {
      font-size: var(--sc-text-xs);
      color: var(--sc-text-faint);
      font-variant-numeric: tabular-nums;
    }
    .message {
      font-size: var(--sc-text-sm);
      color: var(--sc-text);
    }
    .detail {
      font-size: var(--sc-text-xs);
      color: var(--sc-text-muted);
    }
    .content {
      display: flex;
      flex-direction: column;
      gap: var(--sc-space-2xs);
    }
    .empty {
      font-size: var(--sc-text-sm);
      color: var(--sc-text-muted);
      padding: var(--sc-space-lg) 0;
    }
    @media (prefers-reduced-motion: reduce) {
      .entry {
        animation: none;
      }
    }
  `;

  override render() {
    const visible = this.items.slice(0, this.max);
    return html`
      <div class="timeline">
        ${visible.map(
          (item, i) => html`
            <div class="entry" style="animation-delay: ${i * 50}ms">
              <div class="dot ${item.status}"></div>
              <div class="line"></div>
              <div class="content">
                <span class="time">${item.time}</span>
                <span class="message">${item.message}</span>
                ${item.detail ? html`<span class="detail">${item.detail}</span>` : nothing}
              </div>
            </div>
          `,
        )}
      </div>
      ${this.items.length === 0 ? html`<div class="empty">No recent activity</div>` : nothing}
    `;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "sc-timeline": ScTimeline;
  }
}
