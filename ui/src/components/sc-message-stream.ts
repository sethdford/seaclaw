import { LitElement, html, css, nothing } from "lit";
import { customElement, property } from "lit/decorators.js";
import type { TemplateResult } from "lit";

function escapeHtml(s: string): string {
  return s
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

function processInline(str: string): TemplateResult {
  const segments: (string | TemplateResult)[] = [];
  let lastIndex = 0;
  const re = /\*\*(.+?)\*\*|\*([^*]+)\*|`([^`]+)`/g;
  let m;
  while ((m = re.exec(str)) !== null) {
    if (m.index > lastIndex) {
      segments.push(escapeHtml(str.slice(lastIndex, m.index)));
    }
    if (m[1]) {
      segments.push(html`<strong>${m[1]}</strong>`);
    } else if (m[2]) {
      segments.push(html`<em>${m[2]}</em>`);
    } else if (m[3]) {
      segments.push(html`<code class="inline">${escapeHtml(m[3])}</code>`);
    }
    lastIndex = re.lastIndex;
  }
  if (lastIndex < str.length) {
    segments.push(escapeHtml(str.slice(lastIndex)));
  }
  return html`${segments}`;
}

function parseMarkdown(text: string, onCopy?: (code: string) => void): TemplateResult[] {
  const parts: TemplateResult[] = [];
  let remaining = text;
  const codeBlockRe = /^```(\w*)\n([\s\S]*?)```/;

  while (remaining.length > 0) {
    const cbMatch = remaining.match(codeBlockRe);
    if (cbMatch && remaining.startsWith("```")) {
      const [full, , code] = cbMatch;
      const codeStr = code.trim();
      parts.push(
        html`<div class="code-block-wrapper">
          <pre class="code-block"><code>${escapeHtml(codeStr)}</code></pre>
          <button
            class="copy-btn"
            type="button"
            aria-label="Copy code"
            @click=${() => onCopy?.(codeStr)}
          >
            Copy
          </button>
        </div>`,
      );
      remaining = remaining.slice(full.length);
      continue;
    }

    const lineEnd = remaining.indexOf("\n");
    const line = lineEnd >= 0 ? remaining.slice(0, lineEnd) : remaining;

    const ulMatch = line.match(/^[\-\*]\s+(.+)$/);
    if (ulMatch) {
      parts.push(html`<li class="md-list-item">${processInline(ulMatch[1])}</li>`);
      remaining = remaining.slice(line.length + (lineEnd >= 0 ? 1 : 0));
      continue;
    }

    if (line.trim() === "") {
      parts.push(html`<br />`);
      remaining = remaining.slice(line.length + (lineEnd >= 0 ? 1 : 0));
      continue;
    }

    parts.push(html`<span class="md-line">${processInline(line)}</span>`);
    remaining = remaining.slice(line.length + (lineEnd >= 0 ? 1 : 0));
  }

  return parts;
}

type MessageRole = "user" | "assistant";

@customElement("sc-message-stream")
export class ScMessageStream extends LitElement {
  @property({ type: String }) content = "";
  @property({ type: Boolean }) streaming = false;
  @property({ type: String }) role: MessageRole = "assistant";

  static override styles = css`
    @keyframes sc-blink {
      0%,
      50% {
        opacity: 1;
      }
      51%,
      100% {
        opacity: 0;
      }
    }

    :host {
      display: block;
    }

    .bubble {
      font-family: var(--sc-font);
      font-size: var(--sc-text-sm);
      line-height: var(--sc-leading-relaxed);
      padding: var(--sc-space-md) var(--sc-space-lg);
      border-radius: var(--sc-radius-xl);
      max-width: 85%;
    }

    .bubble.role-user {
      margin-left: auto;
      background: color-mix(in srgb, var(--sc-accent) 12%, var(--sc-bg-surface));
      border: 1px solid color-mix(in srgb, var(--sc-accent) 20%, var(--sc-border-subtle));
    }

    .bubble.role-assistant {
      margin-right: auto;
      background: color-mix(
        in srgb,
        var(--sc-surface) var(--sc-glass-standard-bg-opacity, 6%),
        transparent
      );
      backdrop-filter: blur(var(--sc-glass-standard-blur, 24px))
        saturate(var(--sc-glass-standard-saturate, 180%));
      -webkit-backdrop-filter: blur(var(--sc-glass-standard-blur, 24px))
        saturate(var(--sc-glass-standard-saturate, 180%));
      border: 1px solid
        color-mix(
          in srgb,
          var(--sc-border) var(--sc-glass-standard-border-opacity, 8%),
          transparent
        );
    }

    .content {
      color: var(--sc-text);
    }

    .content :global(.inline) {
      font-family: var(--sc-font-mono);
      font-size: 0.9em;
      padding: 0.1em 0.35em;
      border-radius: var(--sc-radius-sm);
      background: var(--sc-bg-inset);
    }

    .content :global(.code-block-wrapper) {
      position: relative;
      margin: var(--sc-space-sm) 0;
    }

    .content :global(.code-block) {
      font-family: var(--sc-font-mono);
      font-size: var(--sc-text-sm);
      background: var(--sc-bg-inset);
      padding: var(--sc-space-md);
      border-radius: var(--sc-radius-md);
      overflow-x: auto;
    }

    .content :global(.code-block) code {
      white-space: pre;
    }

    .content :global(.copy-btn) {
      position: absolute;
      top: var(--sc-space-xs);
      right: var(--sc-space-xs);
      padding: var(--sc-space-2xs) var(--sc-space-sm);
      font-family: var(--sc-font);
      font-size: var(--sc-text-xs);
      color: var(--sc-text-muted);
      background: var(--sc-bg-elevated);
      border: 1px solid var(--sc-border-subtle);
      border-radius: var(--sc-radius-sm);
      cursor: pointer;
      opacity: 0;
      transition: opacity var(--sc-duration-fast) var(--sc-ease-out);
    }

    .content :global(.code-block-wrapper:hover .copy-btn) {
      opacity: 1;
    }

    .content :global(.copy-btn:focus) {
      opacity: 1;
    }

    .content :global(.md-list-item) {
      margin-left: var(--sc-space-md);
      margin-bottom: var(--sc-space-2xs);
    }

    .content :global(.md-line) {
      display: block;
      margin-bottom: var(--sc-space-2xs);
    }

    .cursor {
      display: inline-block;
      width: 2px;
      height: 1em;
      background: var(--sc-accent);
      margin-left: 2px;
      vertical-align: text-bottom;
      animation: sc-blink 1s step-end infinite;
    }

    @media (prefers-reduced-motion: reduce) {
      .cursor {
        animation: none;
      }
    }
  `;

  private _copyCode(code: string): void {
    navigator.clipboard?.writeText(code);
  }

  override render() {
    const parsed = parseMarkdown(this.content, (code) => this._copyCode(code));

    return html`
      <div class="bubble role-${this.role}">
        <div class="content">
          ${parsed}
          ${this.streaming ? html`<span class="cursor" aria-hidden="true"></span>` : nothing}
        </div>
      </div>
    `;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "sc-message-stream": ScMessageStream;
  }
}
