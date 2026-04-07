import { LitElement, html, css } from "lit";
import { customElement, property } from "lit/decorators.js";

/** Renders persisted HuLa trace steps as a compact tree list. */
@customElement("hu-hula-tree")
export class HuHulaTree extends LitElement {
  @property({ type: Array }) steps: Record<string, unknown>[] = [];

  static override styles = css`
    :host {
      display: block;
      color: var(--hu-text);
      font-family: var(--hu-font-mono);
      font-size: var(--hu-text-sm);
    }
    ul {
      list-style: none;
      margin: 0;
      padding: 0;
    }
    li {
      padding: var(--hu-space-xs) 0;
      border-bottom: 1px solid var(--hu-border-subtle);
    }
    li:last-child {
      border-bottom: none;
    }
    .id {
      color: var(--hu-accent-text);
      font-weight: var(--hu-weight-medium);
    }
    .meta {
      color: var(--hu-text-muted);
      margin-inline-start: var(--hu-space-sm);
    }
  `;

  override render() {
    if (!this.steps.length) {
      return html`<p class="meta">No trace steps in this record.</p>`;
    }
    return html`
      <ul role="tree" aria-label="HuLa execution trace">
        ${this.steps.map(
          (s) => html`
            <li role="treeitem">
              <span class="id">${String(s.id ?? "?")}</span>
              <span class="meta"
                >${String(s.op ?? "")}${s.tool ? ` · ${String(s.tool)}` : ""} ·
                ${String(s.status ?? "")}</span
              >
            </li>
          `,
        )}
      </ul>
    `;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "hu-hula-tree": HuHulaTree;
  }
}
