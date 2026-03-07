import { LitElement, html, css } from "lit";
import { customElement, property } from "lit/decorators.js";
import { icons } from "../icons.js";

@customElement("sc-pagination")
export class ScPagination extends LitElement {
  @property({ type: Number }) total = 0;
  @property({ type: Number }) page = 1;
  @property({ type: Number }) pageSize = 10;
  @property({ type: Array }) pageSizes: number[] = [10, 25, 50];

  static override get properties() {
    return {
      total: { type: Number },
      page: { type: Number },
      pageSize: { type: Number },
      pageSizes: { type: Array },
    };
  }

  get pageCount(): number {
    if (this.total <= 0 || this.pageSize <= 0) return 0;
    return Math.ceil(this.total / this.pageSize);
  }

  private get _start(): number {
    return this.total === 0 ? 0 : (this.page - 1) * this.pageSize + 1;
  }

  private get _end(): number {
    return Math.min(this.page * this.pageSize, this.total);
  }

  private get _pageNumbers(): (number | "ellipsis")[] {
    const count = this.pageCount;
    if (count <= 7) {
      return Array.from({ length: count }, (_, i) => i + 1);
    }
    const p = this.page;
    if (p <= 4) {
      return [1, 2, 3, 4, 5, "ellipsis", count];
    }
    if (p >= count - 3) {
      return [1, "ellipsis", count - 4, count - 3, count - 2, count - 1, count];
    }
    return [1, "ellipsis", p - 1, p, p + 1, "ellipsis", count];
  }

  static override styles = css`
    :host {
      display: block;
      font-family: var(--sc-font);
    }

    .pagination {
      display: flex;
      flex-wrap: wrap;
      align-items: center;
      gap: var(--sc-space-sm);
      font-size: var(--sc-text-sm);
      color: var(--sc-text-muted);
    }

    .label {
      margin-right: var(--sc-space-sm);
    }

    .nav {
      display: flex;
      align-items: center;
      gap: var(--sc-space-xs);
    }

    button {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      min-width: 2rem;
      min-height: 2rem;
      padding: var(--sc-space-xs) var(--sc-space-sm);
      font-family: var(--sc-font);
      font-size: var(--sc-text-sm);
      background: var(--sc-bg-surface);
      border: 1px solid var(--sc-border-subtle);
      border-radius: var(--sc-radius);
      color: var(--sc-text);
      cursor: pointer;
      transition:
        background var(--sc-duration-fast) var(--sc-ease-out),
        border-color var(--sc-duration-fast) var(--sc-ease-out),
        color var(--sc-duration-fast) var(--sc-ease-out);
    }

    @media (prefers-reduced-motion: reduce) {
      button {
        transition: none;
      }
    }

    button:hover:not(:disabled) {
      background: var(--sc-bg-elevated);
      border-color: var(--sc-border);
    }

    button:focus-visible {
      outline: var(--sc-focus-ring-width) solid var(--sc-focus-ring);
      outline-offset: var(--sc-focus-ring-offset);
    }

    button:disabled {
      opacity: var(--sc-opacity-disabled);
      cursor: not-allowed;
    }

    button.active {
      background: var(--sc-accent);
      border-color: var(--sc-accent);
      color: var(--sc-on-accent);
    }

    button.active:hover:not(:disabled) {
      background: var(--sc-accent-hover);
      border-color: var(--sc-accent-hover);
    }

    .ellipsis {
      padding: 0 var(--sc-space-xs);
      color: var(--sc-text-muted);
    }

    .page-size {
      display: flex;
      align-items: center;
      gap: var(--sc-space-xs);
    }

    .page-size label {
      font-size: var(--sc-text-sm);
      color: var(--sc-text-muted);
    }

    select {
      width: auto;
      min-width: 4rem;
      padding: var(--sc-space-xs) var(--sc-space-sm);
      font-family: var(--sc-font);
      font-size: var(--sc-text-sm);
      background: var(--sc-bg-surface);
      border: 1px solid var(--sc-border-subtle);
      border-radius: var(--sc-radius);
      color: var(--sc-text);
      cursor: pointer;
      appearance: none;
      -webkit-appearance: none;
      -moz-appearance: none;
      background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='12' viewBox='0 0 256 256' fill='%23666'%3E%3Cpath d='M213.66,101.66l-80,80a8,8,0,0,1-11.32,0l-80-80a8,8,0,0,1,11.32-11.32L128,164.69l74.34-74.35a8,8,0,0,1,11.32,11.32Z'/%3E%3C/svg%3E");
      background-repeat: no-repeat;
      background-position: right var(--sc-space-sm) center;
      padding-right: var(--sc-space-xl);
    }

    select:focus-visible {
      outline: var(--sc-focus-ring-width) solid var(--sc-focus-ring);
      outline-offset: var(--sc-focus-ring-offset);
    }

    .icon {
      display: inline-flex;
      width: 1rem;
      height: 1rem;
    }

    .icon svg {
      width: 1rem;
      height: 1rem;
    }
  `;

  private _goToPage(p: number): void {
    if (p < 1 || p > this.pageCount) return;
    this.dispatchEvent(
      new CustomEvent("sc-page-change", {
        bubbles: true,
        composed: true,
        detail: { page: p, pageSize: this.pageSize },
      }),
    );
  }

  private _onPageSizeChange(e: Event): void {
    const target = e.target as HTMLSelectElement;
    const size = parseInt(target.value, 10);
    this.dispatchEvent(
      new CustomEvent("sc-page-change", {
        bubbles: true,
        composed: true,
        detail: { page: 1, pageSize: size },
      }),
    );
  }

  override render() {
    const count = this.pageCount;
    if (count === 0 && this.total === 0) {
      return html`<div class="pagination"><span class="label">Showing 0 of 0</span></div>`;
    }

    return html`
      <div class="pagination" role="navigation" aria-label="Pagination">
        <span class="label">Showing ${this._start}–${this._end} of ${this.total}</span>
        <div class="nav">
          <button
            type="button"
            aria-label="Previous page"
            ?disabled=${this.page <= 1}
            @click=${() => this._goToPage(this.page - 1)}
          >
            <span class="icon" aria-hidden="true">${icons["caret-left"]}</span>
          </button>
          ${this._pageNumbers.map((n) =>
            n === "ellipsis"
              ? html`<span class="ellipsis" aria-hidden="true">…</span>`
              : html`
                  <button
                    type="button"
                    class=${this.page === n ? "active" : ""}
                    aria-label=${`Page ${n}`}
                    aria-current=${this.page === n ? "page" : undefined}
                    @click=${() => this._goToPage(n)}
                  >
                    ${n}
                  </button>
                `,
          )}
          <button
            type="button"
            aria-label="Next page"
            ?disabled=${this.page >= count}
            @click=${() => this._goToPage(this.page + 1)}
          >
            <span class="icon" aria-hidden="true">${icons["caret-right"]}</span>
          </button>
        </div>
        <div class="page-size">
          <label for="page-size-select">Per page</label>
          <select
            id="page-size-select"
            aria-label="Items per page"
            .value=${String(this.pageSize)}
            @change=${this._onPageSizeChange}
          >
            ${this.pageSizes.map((s) => html`<option value=${s}>${s}</option>`)}
          </select>
        </div>
      </div>
    `;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "sc-pagination": ScPagination;
  }
}
