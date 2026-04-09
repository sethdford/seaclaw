import { LitElement, html, css, nothing } from "lit";
import { customElement, property } from "lit/decorators.js";

@customElement("hu-metric-row")
export class ScMetricRow extends LitElement {
  @property({ type: Array })
  items: Array<{ label: string; value: string; accent?: string; countTarget?: number }> = [];

  static override styles = css`
    :host {
      display: block;
      width: 100%;
      contain: layout style;
      container-type: inline-size;
    }
    .row {
      display: flex;
      align-items: center;
      gap: var(--hu-space-lg);
      padding: var(--hu-space-md) 0;
      flex-wrap: wrap;
    }

    .divider {
      width: 1px;
      height: 24px;
      background: var(--hu-border-subtle);
      flex-shrink: 0;
    }

    .item {
      display: flex;
      flex-direction: column;
      gap: var(--hu-space-2xs);
    }

    .item-label {
      font-size: var(--hu-text-xs);
      color: var(--hu-text-muted);
    }

    .item-value {
      font-size: var(--hu-text-sm);
      font-weight: var(--hu-weight-medium);
      color: var(--hu-text);
      font-variant-numeric: tabular-nums;
      &.success {
        color: var(--hu-success);
      }
      &.error {
        color: var(--hu-error);
      }
      &.tertiary {
        color: var(--hu-accent-tertiary-text);
      }
    }

    @container (max-width: 30rem) /* cq-sm */ {
      .divider {
        display: none;
      }

      .row {
        gap: var(--hu-space-md);
      }
    }
  `;

  override render() {
    return html`
      <div class="row">
        ${this.items.map(
          (item, i) => html`
            ${i > 0 ? html`<div class="divider"></div>` : nothing}
            <div class="item" role="group" aria-label="${item.label}: ${item.value}">
              <span class="item-label">${item.label}</span>
              <span
                class="item-value ${item.accent || ""}"
                ?data-count-target=${item.countTarget}
                data-format=${item.countTarget != null ? "number" : nothing}
                role=${item.countTarget != null ? "status" : nothing}
                aria-live=${item.countTarget != null ? "polite" : nothing}
                >${item.countTarget != null ? "0" : item.value}</span
              >
            </div>
          `,
        )}
      </div>
    `;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "hu-metric-row": ScMetricRow;
  }
}
