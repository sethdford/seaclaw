import { LitElement, html, css } from "lit";
import { customElement } from "lit/decorators.js";

/**
 * Responsive grid layout for stat cards. Uses a slot for children (typically
 * hu-stat-card elements). Encapsulates grid and breakpoint CSS in Shadow DOM.
 */
@customElement("hu-stats-row")
export class ScStatsRow extends LitElement {
  static override styles = css`
    :host {
      display: block;
      contain: layout style;
      container-type: inline-size;
    }

    .stats-row {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(11.25rem, 1fr));
      gap: var(--hu-space-md);
      margin-bottom: var(--hu-space-2xl);
    }

    @container (max-width: 40rem) /* cq-compact */ {
      .stats-row {
        grid-template-columns: 1fr 1fr;
      }
    }

    @container (max-width: 30rem) /* cq-sm */ {
      .stats-row {
        grid-template-columns: 1fr;
      }
    }
  `;

  override render() {
    return html`<div class="stats-row" role="group"><slot></slot></div>`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "hu-stats-row": ScStatsRow;
  }
}
