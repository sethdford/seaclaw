import { LitElement, html, css, nothing } from "lit";
import { customElement, property, state } from "lit/decorators.js";
import type { PropertyValues } from "lit";
import { icons } from "../icons.js";
import { renderMarkdown } from "../lib/markdown.js";
import "./hu-code-block.js";

const PREVIEW_LENGTH = 100;
const LONG_CONTENT_THRESHOLD = 200;
const AUTO_COLLAPSE_DELAY_MS = 1000;

@customElement("hu-reasoning-block")
export class ScReasoningBlock extends LitElement {
  @property({ type: String }) content = "";
  @property({ type: Boolean }) streaming = false;
  @property({ type: String }) duration = "";
  @property({ type: Boolean }) collapsed = true;

  @state() private _elapsedDisplay = "";

  private _streamingStartedAt = 0;
  private _rafId = 0;
  private _collapseTimer: number | null = null;

  static override styles = css`
    @keyframes hu-pulse {
      0%,
      100% {
        opacity: 1;
      }
      50% {
        opacity: 0.5;
      }
    }

    @keyframes hu-thinking-shimmer {
      from {
        background-position: -200% center;
      }
      to {
        background-position: 200% center;
      }
    }

    @keyframes hu-reasoning-enter {
      from {
        opacity: 0;
        transform: translateY(-0.5rem);
      }
      to {
        opacity: 1;
        transform: translateY(0);
      }
    }

    :host {
      display: block;
      contain: layout style;
      animation: hu-reasoning-enter var(--hu-duration-normal) var(--hu-ease-out);
    }

    .reasoning-block {
      background: color-mix(in srgb, var(--hu-accent) 4%, transparent);
      border-left: 0.125rem solid color-mix(in srgb, var(--hu-accent) 30%, transparent);
      border-radius: var(--hu-radius-md);
      overflow: hidden;
    }

    .header {
      display: flex;
      align-items: center;
      gap: var(--hu-space-sm);
      padding: var(--hu-space-sm) var(--hu-space-md);
      font-family: var(--hu-font);
      font-size: var(--hu-text-sm);
      color: var(--hu-text-muted);
      cursor: pointer;
      user-select: none;
      width: 100%;
      text-align: left;
      background: transparent;
      border: none;
      transition: background-color var(--hu-duration-fast) var(--hu-ease-out);
    }

    .header.streaming {
      background: linear-gradient(
        90deg,
        transparent 0%,
        color-mix(in srgb, var(--hu-accent) 6%, transparent) 50%,
        transparent 100%
      );
      background-size: 200% 100%;
      animation: hu-thinking-shimmer calc(var(--hu-duration-slowest) * 3) var(--hu-ease-in-out)
        infinite;
    }

    .header:hover {
      background: color-mix(in srgb, var(--hu-accent) 8%, transparent);
    }

    .header.streaming:hover {
      background:
        linear-gradient(
          90deg,
          transparent 0%,
          color-mix(in srgb, var(--hu-accent) 6%, transparent) 50%,
          transparent 100%
        ),
        color-mix(in srgb, var(--hu-accent) 8%, transparent);
      background-size:
        200% 100%,
        auto;
      animation: hu-thinking-shimmer calc(var(--hu-duration-slowest) * 3) var(--hu-ease-in-out)
        infinite;
    }

    .header:focus-visible {
      outline: var(--hu-focus-ring-width) solid var(--hu-focus-ring);
      outline-offset: var(--hu-focus-ring-offset);
    }

    .caret {
      display: inline-flex;
      width: 1em;
      height: 1em;
      flex-shrink: 0;
      transition: transform var(--hu-duration-normal) var(--hu-ease-out);
    }

    .caret.expanded {
      transform: rotate(90deg);
    }

    .brain-icon {
      display: inline-flex;
      width: 1em;
      height: 1em;
      flex-shrink: 0;
      color: var(--hu-accent);
    }

    .pulse-dot {
      width: var(--hu-space-xs);
      height: var(--hu-space-xs);
      border-radius: 50%;
      background: var(--hu-accent);
      animation: hu-pulse var(--hu-duration-slow) var(--hu-ease-in-out) infinite;
    }

    .preview-row {
      padding: 0 var(--hu-space-md) var(--hu-space-sm);
      font-family: var(--hu-font);
      font-size: var(--hu-text-sm);
    }

    .content {
      font-family: var(--hu-font);
      font-size: var(--hu-text-sm);
      color: var(--hu-text);
      overflow: hidden;
      opacity: 1;
      transform: translateY(0);
      transition:
        max-height var(--hu-duration-normal) var(--hu-ease-out),
        opacity var(--hu-duration-normal) var(--hu-ease-out),
        transform var(--hu-duration-normal) var(--hu-ease-out),
        padding var(--hu-duration-normal) var(--hu-ease-out);
    }

    .content.expanded {
      max-height: 125rem;
      padding: 0 var(--hu-space-md) var(--hu-space-md);
      opacity: 1;
      transform: translateY(0);
    }

    .content.collapsed {
      max-height: 0;
      padding: 0;
      opacity: 0;
      transform: translateY(-0.25rem);
    }

    .preview {
      color: var(--hu-text-muted);
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
    }

    .md-content {
      color: var(--hu-text);
    }

    .md-heading {
      font-weight: var(--hu-weight-semibold);
      margin: var(--hu-space-sm) 0 var(--hu-space-xs);
    }

    .md-heading:first-child {
      margin-top: 0;
    }

    .md-content h1.md-heading {
      font-size: var(--hu-text-2xl);
    }
    .md-content h2.md-heading {
      font-size: var(--hu-text-xl);
    }
    .md-content h3.md-heading {
      font-size: var(--hu-text-lg);
    }
    .md-content h4.md-heading,
    .md-content h5.md-heading,
    .md-content h6.md-heading {
      font-size: var(--hu-text-base);
    }

    .md-paragraph {
      margin: var(--hu-space-xs) 0;
    }

    .md-blockquote {
      border-left: 0.1875rem solid var(--hu-accent);
      padding-left: var(--hu-space-md);
      margin: var(--hu-space-sm) 0;
      color: var(--hu-text-muted);
      font-style: italic;
    }

    .md-list {
      margin: var(--hu-space-sm) 0;
      padding-left: var(--hu-space-lg);
    }

    .md-list-item {
      margin-bottom: var(--hu-space-2xs);
    }

    .md-content a {
      color: var(--hu-accent);
      text-decoration: none;
    }

    .md-content a:hover {
      text-decoration: underline;
    }

    .md-content code.inline {
      font-family: var(--hu-font-mono);
      font-size: 0.9em;
      padding: 0.1em 0.35em;
      border-radius: var(--hu-radius-sm);
      background: var(--hu-bg-inset);
    }

    @media (prefers-reduced-motion: reduce) {
      :host {
        animation: none;
        opacity: 1;
      }

      .caret {
        transition: none;
      }

      .content {
        transition: none;
        opacity: 1;
        transform: none;
      }

      .content.collapsed {
        opacity: 1;
        transform: none;
      }

      .header.streaming,
      .header.streaming:hover {
        animation: none;
        background: color-mix(in srgb, var(--hu-accent) 6%, transparent);
        background-size: auto;
      }

      .pulse-dot {
        animation: none;
        opacity: 1;
      }
    }
  `;

  private get _preview(): string {
    const text = this.content.trim().replace(/\s+/g, " ");
    if (text.length <= PREVIEW_LENGTH) return text;
    return text.slice(0, PREVIEW_LENGTH) + "...";
  }

  private _clearCollapseTimer() {
    if (this._collapseTimer !== null) {
      clearTimeout(this._collapseTimer);
      this._collapseTimer = null;
    }
  }

  private _stopElapsedTicker() {
    if (this._rafId !== 0) {
      cancelAnimationFrame(this._rafId);
      this._rafId = 0;
    }
  }

  private _tickElapsed = () => {
    if (!this.streaming) {
      return;
    }
    const secs = (performance.now() - this._streamingStartedAt) / 1000;
    const next = `${secs.toFixed(1)}s`;
    if (next !== this._elapsedDisplay) {
      this._elapsedDisplay = next;
    }
    this._rafId = requestAnimationFrame(this._tickElapsed);
  };

  private _startElapsedTicker() {
    this._stopElapsedTicker();
    this._streamingStartedAt = performance.now();
    this._rafId = requestAnimationFrame(this._tickElapsed);
  }

  private _scheduleAutoCollapse() {
    this._clearCollapseTimer();
    if (this.content.length <= LONG_CONTENT_THRESHOLD) {
      return;
    }
    this._collapseTimer = window.setTimeout(() => {
      this._collapseTimer = null;
      this.collapsed = true;
    }, AUTO_COLLAPSE_DELAY_MS);
  }

  override disconnectedCallback() {
    this._stopElapsedTicker();
    this._clearCollapseTimer();
    super.disconnectedCallback();
  }

  override willUpdate(changed: PropertyValues<this>) {
    super.willUpdate(changed);
    if (!changed.has("streaming")) {
      return;
    }
    const prev = changed.get("streaming") as boolean | undefined;
    if (!prev && this.streaming) {
      this._clearCollapseTimer();
      this.collapsed = false;
    } else if (prev && !this.streaming) {
      this._elapsedDisplay = "";
    }
  }

  override updated(changed: PropertyValues<this>) {
    super.updated(changed);
    if (!changed.has("streaming")) {
      return;
    }
    const prev = changed.get("streaming") as boolean | undefined;
    if (!prev && this.streaming) {
      this._startElapsedTicker();
    } else if (prev && !this.streaming) {
      this._stopElapsedTicker();
      this._scheduleAutoCollapse();
    }
  }

  private _toggle() {
    this._clearCollapseTimer();
    this.collapsed = !this.collapsed;
  }

  private _onKeyDown(e: KeyboardEvent) {
    if (e.key === "Enter" || e.key === " ") {
      e.preventDefault();
      this._toggle();
    }
  }

  override render() {
    const label = this.streaming
      ? `Thinking · ${this._elapsedDisplay || "0.0s"}`
      : this.duration || "Thinking";
    return html`
      <div class="reasoning-block" role="region" aria-label="AI reasoning">
        <button
          class="header ${this.streaming ? "streaming" : ""}"
          @click=${this._toggle}
          @keydown=${this._onKeyDown}
          aria-expanded=${!this.collapsed}
          aria-controls="reasoning-content"
          aria-label="Toggle reasoning content"
        >
          <span class="caret ${this.collapsed ? "" : "expanded"}">${icons["caret-right"]}</span>
          <span class="brain-icon" aria-hidden="true">${icons.brain}</span>
          <span class="label">${label}</span>
          ${this.streaming ? html`<span class="pulse-dot" aria-hidden="true"></span>` : nothing}
        </button>
        ${this.collapsed
          ? html`<div class="preview-row"><span class="preview">${this._preview}</span></div>`
          : nothing}
        <div
          id="reasoning-content"
          class="content ${this.collapsed ? "collapsed" : "expanded"}"
          role=${this.streaming ? "status" : nothing}
          aria-live=${this.streaming ? "polite" : nothing}
        >
          ${this.collapsed ? nothing : renderMarkdown(this.content, { streaming: this.streaming })}
        </div>
      </div>
    `;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "hu-reasoning-block": ScReasoningBlock;
  }
}
