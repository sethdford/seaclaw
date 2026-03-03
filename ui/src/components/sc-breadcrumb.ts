import { LitElement, html, css } from "lit";
import { customElement, property } from "lit/decorators.js";

export type BreadcrumbItem = {
  label: string;
  href?: string;
};

@customElement("sc-breadcrumb")
export class ScBreadcrumb extends LitElement {
  @property({ type: Array }) items: BreadcrumbItem[] = [];

  static override styles = css`
    :host {
      display: block;
    }

    nav {
      font-size: var(--sc-text-sm);
      font-family: var(--sc-font);
    }

    .list {
      display: flex;
      flex-wrap: wrap;
      align-items: center;
      gap: var(--sc-space-xs);
      list-style: none;
      margin: 0;
      padding: 0;
    }

    .item {
      display: flex;
      align-items: center;
      gap: var(--sc-space-xs);
    }

    .item:not(:last-child)::after {
      content: "/";
      color: var(--sc-text-faint);
      margin-left: var(--sc-space-xs);
    }

    .link {
      color: var(--sc-text-muted);
      text-decoration: none;
      transition: color var(--sc-duration-fast) var(--sc-ease-out);
    }

    .link:hover {
      color: var(--sc-accent);
    }

    .link:focus-visible {
      outline: var(--sc-focus-ring-width) solid var(--sc-focus-ring);
      outline-offset: var(--sc-focus-ring-offset);
    }

    .current {
      color: var(--sc-text);
    }

    @media (prefers-reduced-motion: reduce) {
      .link {
        transition: none;
      }
    }
  `;

  private _onClick(e: Event, item: BreadcrumbItem): void {
    if (item.href) {
      e.preventDefault();
      this.dispatchEvent(
        new CustomEvent("sc-navigate", {
          bubbles: true,
          composed: true,
          detail: { href: item.href },
        }),
      );
    }
  }

  override render() {
    return html`
      <nav aria-label="Breadcrumb">
        <ol class="list">
          ${this.items.map((item, i) => {
            const isLast = i === this.items.length - 1;
            return html`
              <li class="item">
                ${isLast
                  ? html`<span class="current" aria-current="page">${item.label}</span>`
                  : item.href
                    ? html`
                        <a
                          class="link"
                          href=${item.href}
                          @click=${(e: Event) => this._onClick(e, item)}
                          >${item.label}</a
                        >
                      `
                    : html`<span class="current">${item.label}</span>`}
              </li>
            `;
          })}
        </ol>
      </nav>
    `;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "sc-breadcrumb": ScBreadcrumb;
  }
}
