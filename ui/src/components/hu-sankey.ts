import { LitElement, html, css } from "lit";
import { customElement, property, state } from "lit/decorators.js";

export interface SankeyNode {
  id: string;
  label: string;
  column?: number;
}

export interface SankeyLink {
  from: string;
  to: string;
  value: number;
}

@customElement("hu-sankey")
export class HuSankey extends LitElement {
  @property({ attribute: false }) nodes: SankeyNode[] = [];
  @property({ attribute: false }) links: SankeyLink[] = [];

  @state() private _hoverLink: number | null = null;

  static override styles = css`
    :host {
      display: block;
      font-family: var(--hu-font);
    }
    svg {
      display: block;
      width: 100%;
      height: auto;
    }
    .node {
      fill: var(--hu-surface-container-high);
      stroke: var(--hu-border-subtle);
      stroke-width: 1;
    }
    .node-label {
      fill: var(--hu-text);
      font-size: var(--hu-text-xs);
      pointer-events: none;
    }
    .link {
      fill: none;
      stroke-opacity: 0.35;
      transition: stroke-opacity var(--hu-duration-fast) var(--hu-ease-out);
    }
    .link:hover,
    .link.active {
      stroke-opacity: 0.65;
    }
    @media (prefers-reduced-motion: reduce) {
      .link {
        transition: none;
      }
    }
  `;

  private _color(i: number): string {
    const n = (i % 16) + 1;
    return `var(--hu-chart-categorical-${n})`;
  }

  private _layout(): {
    positions: Map<string, { x: number; y: number; w: number; h: number }>;
    linkPaths: Array<{ d: string; stroke: string; w: number; key: string }>;
    vbW: number;
    vbH: number;
  } | null {
    const nodes = this.nodes ?? [];
    const links = this.links ?? [];
    if (nodes.length === 0) return null;

    const byCol = new Map<number, SankeyNode[]>();
    let maxCol = 0;
    for (const n of nodes) {
      const c = n.column ?? 0;
      maxCol = Math.max(maxCol, c);
      if (!byCol.has(c)) byCol.set(c, []);
      byCol.get(c)!.push(n);
    }

    const colW = 100;
    const gapY = 12;
    const nodeH = 36;
    const pad = 16;
    const vbW = (maxCol + 1) * colW + pad * 2;
    let maxRows = 0;
    for (const [, arr] of byCol) maxRows = Math.max(maxRows, arr.length);
    const vbH = Math.max(160, maxRows * (nodeH + gapY) + pad * 2);

    const positions = new Map<string, { x: number; y: number; w: number; h: number }>();
    for (let col = 0; col <= maxCol; col++) {
      const arr = byCol.get(col) ?? [];
      const totalH = arr.length * (nodeH + gapY) - gapY;
      const y0 = (vbH - totalH) / 2;
      arr.forEach((n, i) => {
        const x = pad + col * colW;
        const y = y0 + i * (nodeH + gapY);
        positions.set(n.id, { x, y, w: colW - 24, h: nodeH });
      });
    }

    const nodeIndex = new Map(nodes.map((n, i) => [n.id, i]));
    const linkPaths: Array<{ d: string; stroke: string; w: number; key: string }> = [];

    for (const lk of links) {
      const a = positions.get(lk.from);
      const b = positions.get(lk.to);
      if (!a || !b) continue;
      const x1 = a.x + a.w;
      const y1 = a.y + a.h / 2;
      const x2 = b.x;
      const y2 = b.y + b.h / 2;
      const mx = (x1 + x2) / 2;
      const strokeW = Math.max(2, Math.min(24, 4 + lk.value * 0.5));
      const d = `M ${x1} ${y1} C ${mx} ${y1}, ${mx} ${y2}, ${x2} ${y2}`;
      const ci = nodeIndex.get(lk.from) ?? 0;
      linkPaths.push({
        d,
        stroke: this._color(ci),
        w: strokeW,
        key: `${lk.from}-${lk.to}`,
      });
    }

    return { positions, linkPaths, vbW, vbH };
  }

  override render() {
    const L = this._layout();
    if (!L) {
      return html`<div role="status" style="color:var(--hu-text-muted)">No Sankey data</div>`;
    }
    const { positions, linkPaths, vbW, vbH } = L;

    return html`
      <svg viewBox="0 0 ${vbW} ${vbH}" role="img" aria-label="Flow diagram">
        ${linkPaths.map(
          (lk, i) => html`
            <path
              class="link ${this._hoverLink === i ? "active" : ""}"
              d=${lk.d}
              stroke=${lk.stroke}
              stroke-width=${lk.w}
              @mouseenter=${() => (this._hoverLink = i)}
              @mouseleave=${() => (this._hoverLink = null)}
            />
          `,
        )}
        ${[...positions.entries()].map(([id, p]) => {
          const n = this.nodes.find((x) => x.id === id);
          return html`
            <rect class="node" x=${p.x} y=${p.y} width=${p.w} height=${p.h} rx="6" />
            <text class="node-label" x=${p.x + 8} y=${p.y + p.h / 2 + 4}>${n?.label ?? id}</text>
          `;
        })}
      </svg>
    `;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "hu-sankey": HuSankey;
  }
}
