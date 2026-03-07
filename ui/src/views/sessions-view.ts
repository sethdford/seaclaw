import { LitElement, css } from "lit";
import { customElement } from "lit/decorators.js";

@customElement("sc-sessions-view")
export class ScSessionsView extends LitElement {
  static override styles = css`
    :host {
      view-transition-name: view-sessions;
      display: block;
    }
  `;
  override connectedCallback(): void {
    super.connectedCallback();
    this.dispatchEvent(
      new CustomEvent("navigate", {
        detail: "chat",
        bubbles: true,
        composed: true,
      }),
    );
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "sc-sessions-view": ScSessionsView;
  }
}
