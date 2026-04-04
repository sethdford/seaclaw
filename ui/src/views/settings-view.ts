import { LitElement, html, css, nothing } from "lit";
import type { TemplateResult } from "lit";
import { customElement, state } from "lit/decorators.js";
import { icons } from "../icons.js";
import "../components/hu-page-hero.js";

type SectionId =
  | "config" | "models" | "tools" | "channels" | "skills" | "automations"
  | "security" | "nodes" | "usage" | "observability" | "logs" | "sessions"
  | "overview" | "turing" | "hula" | "design-system";

interface SettingsSection {
  id: SectionId;
  label: string;
  icon: ReturnType<typeof html>;
  load: () => Promise<unknown>;
  render: () => TemplateResult;
}

const SETTINGS_SECTIONS: SettingsSection[] = [
  { id: "config", label: "Config", icon: icons.settings, load: () => import("./config-view.js"), render: () => html`<hu-config-view></hu-config-view>` },
  { id: "models", label: "Models", icon: icons.cpu, load: () => import("./models-view.js"), render: () => html`<hu-models-view></hu-models-view>` },
  { id: "tools", label: "Tools", icon: icons.wrench, load: () => import("./tools-view.js"), render: () => html`<hu-tools-view></hu-tools-view>` },
  { id: "channels", label: "Channels", icon: icons.radio, load: () => import("./channels-view.js"), render: () => html`<hu-channels-view></hu-channels-view>` },
  { id: "skills", label: "Skills", icon: icons.puzzle, load: () => import("./skills-view.js"), render: () => html`<hu-skills-view></hu-skills-view>` },
  { id: "automations", label: "Automations", icon: icons.timer, load: () => import("./automations-view.js"), render: () => html`<hu-automations-view></hu-automations-view>` },
  { id: "security", label: "Security", icon: icons.shield, load: () => import("./security-view.js"), render: () => html`<hu-security-view></hu-security-view>` },
  { id: "nodes", label: "Nodes", icon: icons.server, load: () => import("./nodes-view.js"), render: () => html`<hu-nodes-view></hu-nodes-view>` },
  { id: "usage", label: "Usage", icon: icons["bar-chart"], load: () => import("./usage-view.js"), render: () => html`<hu-usage-view></hu-usage-view>` },
  { id: "observability", label: "Observability", icon: icons["chart-line"], load: () => import("./metrics-view.js"), render: () => html`<hu-metrics-view></hu-metrics-view>` },
  { id: "logs", label: "Logs", icon: icons.terminal, load: () => import("./logs-view.js"), render: () => html`<hu-logs-view></hu-logs-view>` },
  { id: "sessions", label: "Sessions", icon: icons["clock-counter-clockwise"], load: () => import("./sessions-view.js"), render: () => html`<hu-sessions-view></hu-sessions-view>` },
  { id: "overview", label: "Overview", icon: icons.grid, load: () => import("./overview-view.js"), render: () => html`<hu-overview-view></hu-overview-view>` },
  { id: "turing", label: "Turing", icon: icons["chart-line"], load: () => import("./turing-view.js"), render: () => html`<hu-turing-view></hu-turing-view>` },
  { id: "hula", label: "HuLa", icon: icons.code, load: () => import("./hula-view.js"), render: () => html`<hu-hula-view></hu-hula-view>` },
  { id: "design-system", label: "Design System", icon: icons.grid, load: () => import("./design-system-view.js"), render: () => html`<hu-design-system-view></hu-design-system-view>` },
];

@customElement("hu-settings-view")
export class ScSettingsView extends LitElement {
  static override styles = css`
    :host {
      display: block;
    }

    .header-area {
      padding: var(--hu-space-lg) var(--hu-space-lg) 0;
      max-width: 72rem;
      margin: 0 auto;
    }

    .tabs {
      display: flex;
      gap: var(--hu-space-2xs);
      flex-wrap: wrap;
      padding: 0 var(--hu-space-lg);
      max-width: 72rem;
      margin: 0 auto var(--hu-space-md);
      border-bottom: 1px solid var(--hu-border-subtle);
      padding-bottom: var(--hu-space-sm);
    }

    .tab-btn {
      display: flex;
      align-items: center;
      gap: var(--hu-space-2xs);
      padding: var(--hu-space-2xs) var(--hu-space-sm);
      background: transparent;
      border: none;
      border-radius: var(--hu-radius-sm);
      font-size: var(--hu-text-xs);
      font-family: var(--hu-font);
      color: var(--hu-text-muted);
      cursor: pointer;
      white-space: nowrap;
      transition:
        background var(--hu-duration-fast),
        color var(--hu-duration-fast);
    }

    .tab-btn:hover {
      background: var(--hu-hover-overlay);
      color: var(--hu-text);
    }

    .tab-btn:focus-visible {
      outline: var(--hu-focus-ring-width) solid var(--hu-accent);
      outline-offset: calc(-1 * var(--hu-focus-ring-width));
    }

    .tab-btn[aria-selected="true"] {
      color: var(--hu-accent-text, var(--hu-accent));
      background: var(--hu-surface-container-high);
    }

    .tab-btn .icon {
      width: var(--hu-icon-xs);
      height: var(--hu-icon-xs);
    }

    .tab-btn .icon svg {
      width: 100%;
      height: 100%;
    }

    .content {
      min-height: 20rem;
    }

    .loading {
      display: flex;
      align-items: center;
      justify-content: center;
      min-height: 20rem;
      color: var(--hu-text-muted);
      font-size: var(--hu-text-sm);
    }
  `;

  @state() private _activeSection: SectionId = "config";
  @state() private _loading = false;
  private _loadedSections = new Set<SectionId>();

  override render() {
    const active = SETTINGS_SECTIONS.find((s) => s.id === this._activeSection) ?? SETTINGS_SECTIONS[0];
    return html`
      <div class="header-area">
        <hu-page-hero heading="Settings" subheading="Configure your h-uman instance"></hu-page-hero>
      </div>

      <nav class="tabs" role="tablist" aria-label="Settings sections">
        ${SETTINGS_SECTIONS.map(
          (s) => html`
            <button
              class="tab-btn"
              role="tab"
              aria-selected=${String(s.id === this._activeSection)}
              @click=${() => void this._selectSection(s.id)}
            >
              <span class="icon">${s.icon}</span>
              ${s.label}
            </button>
          `,
        )}
      </nav>

      <div class="content" role="tabpanel" aria-label=${active.label}>
        ${this._loading ? html`<div class="loading">Loading...</div>` : this._renderSection(active)}
      </div>
    `;
  }

  private _renderSection(section: SettingsSection) {
    if (!this._loadedSections.has(section.id)) return nothing;
    return section.render();
  }

  private async _selectSection(id: SectionId): Promise<void> {
    if (id === this._activeSection && this._loadedSections.has(id)) return;
    this._activeSection = id;
    const section = SETTINGS_SECTIONS.find((s) => s.id === id);
    if (!section) return;
    if (!this._loadedSections.has(id)) {
      this._loading = true;
      await section.load();
      this._loadedSections.add(id);
      this._loading = false;
    }
  }

  override async firstUpdated(): Promise<void> {
    await this._selectSection(this._activeSection);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "hu-settings-view": ScSettingsView;
  }
}
