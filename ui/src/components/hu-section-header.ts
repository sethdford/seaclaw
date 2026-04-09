import { LitElement, html, css, nothing } from "lit";
import { customElement, property } from "lit/decorators.js";

@customElement("hu-section-header")
export class ScSectionHeader extends LitElement {
  @property({ type: String }) heading = "";
  @property({ type: String }) description = "";

  static override styles = css`
    :host {
      display: block;
      width: 100%;
      contain: layout style;
      container-type: inline-size;
    }
    .header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: var(--hu-space-md);
      margin-bottom: var(--hu-space-md);
    }

    .text {
      flex: 1;
      min-width: 0;
    }

    .heading {
      margin: 0;
      font-size: clamp(var(--hu-text-lg), 3cqi, var(--hu-text-xl));
      font-weight: var(--hu-weight-semibold);
      color: var(--hu-text);
    }

    .description {
      margin: var(--hu-space-xs) 0 0;
      font-size: var(--hu-text-sm);
      color: var(--hu-text-muted);
    }

    .actions {
      display: flex;
      align-items: center;
      gap: var(--hu-space-sm);
      flex-shrink: 0;
    }

    .actions ::slotted(*) {
      flex-shrink: 0;
    }

    @container (max-width: 40rem) /* cq-compact */ {
      .header {
        flex-direction: column;
        align-items: flex-start;
        gap: var(--hu-space-md);
      }
      .actions {
        width: 100%;
        flex-wrap: wrap;
      }
    }
  `;

  override render() {
    return html`
      <div class="header">
        <div class="text">
          <h2 class="heading">${this.heading}</h2>
          ${this.description ? html`<p class="description">${this.description}</p>` : nothing}
        </div>
        <div class="actions">
          <slot></slot>
        </div>
      </div>
    `;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "hu-section-header": ScSectionHeader;
  }
}
