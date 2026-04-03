import { LitElement, html, css } from "lit";
import { customElement, property, state } from "lit/decorators.js";
import { EditorState } from "@codemirror/state";
import { EditorView, keymap, lineNumbers, highlightActiveLine } from "@codemirror/view";
import { defaultKeymap, history, historyKeymap } from "@codemirror/commands";
import { html as htmlLang } from "@codemirror/lang-html";
import { javascript } from "@codemirror/lang-javascript";
import { markdown as markdownLang } from "@codemirror/lang-markdown";
import type { CanvasFormat } from "./hu-canvas.js";

/**
 * CodeMirror 6 editor for canvas source code. Emits "canvas-content-changed"
 * custom events on edits (debounced externally by the parent view).
 */
@customElement("hu-canvas-editor")
export class HuCanvasEditor extends LitElement {
  @property({ type: String }) content = "";
  @property({ type: String }) format: CanvasFormat = "html";
  @property({ type: Boolean }) readonly = false;

  @state() private _ready = false;

  private _view: EditorView | null = null;
  private _updating = false;

  static override styles = css`
    :host {
      display: flex;
      flex-direction: column;
      flex: 1;
      min-height: 0;
      overflow: hidden;
    }

    .editor-container {
      flex: 1;
      min-height: 0;
      overflow: auto;
    }

    .cm-editor {
      height: 100%;
    }

    .cm-editor .cm-scroller {
      font-family: var(--hu-font-mono);
      font-size: var(--hu-text-sm);
    }

    .cm-editor .cm-content {
      caret-color: var(--hu-accent);
    }

    .cm-editor .cm-activeLine {
      background: color-mix(in srgb, var(--hu-accent) 5%, transparent);
    }

    .cm-editor .cm-gutters {
      background: var(--hu-surface-container);
      border-right: 1px solid var(--hu-border-subtle);
      color: var(--hu-text-secondary);
    }

    .cm-editor .cm-activeLineGutter {
      background: color-mix(in srgb, var(--hu-accent) 8%, transparent);
    }

    .cm-editor .cm-cursor {
      border-left-color: var(--hu-accent);
    }

    .cm-editor .cm-selectionBackground,
    .cm-editor ::selection {
      background: color-mix(in srgb, var(--hu-accent) 20%, transparent) !important;
    }

    .cm-editor.cm-focused {
      outline: none;
    }

    .loading {
      display: flex;
      align-items: center;
      justify-content: center;
      min-height: 8rem;
      font-size: var(--hu-text-sm);
      color: var(--hu-text-secondary);
    }
  `;

  private _getLanguageExtension() {
    switch (this.format) {
      case "react":
        return javascript({ jsx: true });
      case "html":
      case "svg":
      case "mockup":
        return htmlLang();
      case "markdown":
      case "mermaid":
        return markdownLang();
      case "code":
        return javascript();
      default:
        return htmlLang();
    }
  }

  private _buildTheme() {
    return EditorView.theme({
      "&": {
        backgroundColor: "transparent",
        color: "var(--hu-text)",
      },
      ".cm-content": {
        caretColor: "var(--hu-accent)",
      },
      ".cm-gutters": {
        backgroundColor: "var(--hu-surface-container)",
        color: "var(--hu-text-secondary)",
        borderRight: "1px solid var(--hu-border-subtle)",
      },
    });
  }

  override firstUpdated(): void {
    const container = this.renderRoot.querySelector(".editor-container");
    if (!container) return;

    const startState = EditorState.create({
      doc: this.content,
      extensions: [
        lineNumbers(),
        highlightActiveLine(),
        history(),
        keymap.of([...defaultKeymap, ...historyKeymap]),
        this._getLanguageExtension(),
        this._buildTheme(),
        EditorState.readOnly.of(this.readonly),
        EditorView.updateListener.of((update) => {
          if (update.docChanged && !this._updating) {
            const newContent = update.state.doc.toString();
            this.dispatchEvent(
              new CustomEvent("canvas-content-changed", {
                detail: { content: newContent },
                bubbles: true,
                composed: true,
              }),
            );
          }
        }),
      ],
    });

    this._view = new EditorView({
      state: startState,
      parent: container as HTMLElement,
    });
    this._ready = true;
  }

  override updated(changed: Map<string, unknown>): void {
    if (!this._view) return;

    if (changed.has("content") && this.content !== this._view.state.doc.toString()) {
      this._updating = true;
      this._view.dispatch({
        changes: {
          from: 0,
          to: this._view.state.doc.length,
          insert: this.content,
        },
      });
      this._updating = false;
    }

    if (changed.has("format")) {
      const newState = EditorState.create({
        doc: this._view.state.doc.toString(),
        extensions: [
          lineNumbers(),
          highlightActiveLine(),
          history(),
          keymap.of([...defaultKeymap, ...historyKeymap]),
          this._getLanguageExtension(),
          this._buildTheme(),
          EditorState.readOnly.of(this.readonly),
          EditorView.updateListener.of((update) => {
            if (update.docChanged && !this._updating) {
              const newContent = update.state.doc.toString();
              this.dispatchEvent(
                new CustomEvent("canvas-content-changed", {
                  detail: { content: newContent },
                  bubbles: true,
                  composed: true,
                }),
              );
            }
          }),
        ],
      });
      this._view.setState(newState);
    }
  }

  override disconnectedCallback(): void {
    this._view?.destroy();
    this._view = null;
    super.disconnectedCallback();
  }

  override render() {
    return html`
      <div class="editor-container" role="textbox" aria-label="Canvas source editor">
        ${!this._ready ? html`<div class="loading">Loading editor...</div>` : ""}
      </div>
    `;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "hu-canvas-editor": HuCanvasEditor;
  }
}
