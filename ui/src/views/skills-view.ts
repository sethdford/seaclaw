import { LitElement, html, css } from "lit";
import { customElement, state } from "lit/decorators.js";
import type { GatewayClient } from "../gateway.js";

interface Skill {
  name: string;
  description?: string;
  enabled?: boolean;
}

@customElement("sc-skills-view")
export class ScSkillsView extends LitElement {
  static override styles = css`
    :host {
      display: block;
      color: var(--sc-text);
    }
    .header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-bottom: 1rem;
      flex-wrap: wrap;
      gap: 1rem;
    }
    h2 {
      margin: 0;
      font-size: 1.25rem;
      font-weight: 600;
      color: var(--sc-text);
    }
    .install-row {
      display: flex;
      gap: 0.5rem;
      align-items: center;
    }
    .install-row input {
      padding: 0.5rem 0.75rem;
      background: var(--sc-bg);
      border: 1px solid var(--sc-border);
      border-radius: var(--sc-radius);
      color: var(--sc-text);
      font-size: 0.875rem;
      width: 200px;
    }
    .btn {
      padding: 0.5rem 1rem;
      background: var(--sc-accent);
      color: white;
      border: none;
      border-radius: var(--sc-radius);
      cursor: pointer;
      font-size: 0.875rem;
    }
    .btn:hover {
      background: var(--sc-accent-hover);
    }
    .skills-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
      gap: 1rem;
    }
    .skill-card {
      padding: 1rem;
      background: var(--sc-bg-surface);
      border: 1px solid var(--sc-border);
      border-radius: var(--sc-radius);
      display: flex;
      flex-direction: column;
      gap: 0.75rem;
    }
    .skill-name {
      font-weight: 600;
      color: var(--sc-text);
    }
    .skill-desc {
      font-size: 0.875rem;
      color: var(--sc-text-muted);
      flex: 1;
    }
    .skill-footer {
      display: flex;
      align-items: center;
      justify-content: space-between;
    }
    .toggle {
      position: relative;
      width: 44px;
      height: 24px;
      background: var(--sc-border);
      border-radius: 12px;
      cursor: pointer;
      transition: background 0.2s;
    }
    .toggle.enabled {
      background: var(--sc-accent);
    }
    .toggle::after {
      content: "";
      position: absolute;
      top: 2px;
      left: 2px;
      width: 20px;
      height: 20px;
      background: white;
      border-radius: 50%;
      transition: transform 0.2s;
    }
    .toggle.enabled::after {
      transform: translateX(20px);
    }
    .status {
      font-size: 0.75rem;
      color: var(--sc-text-muted);
    }
    .error {
      color: #f87171;
      font-size: 0.875rem;
      margin-bottom: 0.5rem;
    }
  `;

  @state() private skills: Skill[] = [];
  @state() private loading = false;
  @state() private error = "";
  @state() private installUrl = "";

  private get gateway(): GatewayClient | null {
    return (
      (document.querySelector("sc-app") as { gateway?: GatewayClient })
        ?.gateway ?? null
    );
  }

  override connectedCallback(): void {
    super.connectedCallback();
    this.loadSkills();
  }

  private async loadSkills(): Promise<void> {
    const gw = this.gateway;
    if (!gw) return;
    this.loading = true;
    this.error = "";
    try {
      const res = (await gw.request<{ skills?: Skill[] }>(
        "skills.list",
        {},
      )) as { skills?: Skill[] } | { result?: { skills?: Skill[] } };
      const skills =
        (res && "skills" in res && res.skills) ||
        (res && "result" in res && res.result?.skills) ||
        [];
      this.skills = Array.isArray(skills) ? skills : [];
    } catch (e) {
      this.error = e instanceof Error ? e.message : "Failed to load skills";
      this.skills = [];
    } finally {
      this.loading = false;
    }
  }

  private async toggleSkill(skill: Skill): Promise<void> {
    const gw = this.gateway;
    if (!gw) return;
    const enable = !skill.enabled;
    this.error = "";
    try {
      await gw.request(enable ? "skills.enable" : "skills.disable", {
        name: skill.name,
      });
      skill.enabled = enable;
      this.requestUpdate();
    } catch (e) {
      this.error = e instanceof Error ? e.message : "Failed to update skill";
    }
  }

  private async installSkill(): Promise<void> {
    const gw = this.gateway;
    if (!gw || !this.installUrl.trim()) return;
    this.error = "";
    try {
      await gw.request("skills.install", {
        name: this.installUrl.trim(),
      });
      this.installUrl = "";
      await this.loadSkills();
    } catch (e) {
      this.error =
        e instanceof Error
          ? e.message
          : "Install not supported or failed (skills.install may be unavailable)";
    }
  }

  override render() {
    return html`
      <div class="header">
        <h2>Skills</h2>
        <div class="install-row">
          <input
            type="url"
            placeholder="https://..."
            .value=${this.installUrl}
            @input=${(e: Event) =>
              (this.installUrl = (e.target as HTMLInputElement).value)}
          />
          <button class="btn" @click=${this.installSkill}>Install</button>
        </div>
      </div>
      ${this.error ? html`<p class="error">${this.error}</p>` : ""}
      ${this.loading
        ? html`<p style="color: var(--sc-text-muted)">Loading...</p>`
        : this.skills.length === 0
          ? html`<p style="color: var(--sc-text-muted)">
              No skills installed.
            </p>`
          : html`
              <div class="skills-grid">
                ${this.skills.map(
                  (skill) => html`
                    <div class="skill-card">
                      <div class="skill-name">${skill.name}</div>
                      <div class="skill-desc">
                        ${skill.description ?? "No description"}
                      </div>
                      <div class="skill-footer">
                        <span class="status">
                          ${skill.enabled !== false ? "Enabled" : "Disabled"}
                        </span>
                        <div
                          class="toggle ${skill.enabled !== false
                            ? "enabled"
                            : ""}"
                          @click=${() => this.toggleSkill(skill)}
                          role="switch"
                          aria-checked=${skill.enabled !== false}
                        ></div>
                      </div>
                    </div>
                  `,
                )}
              </div>
            `}
    `;
  }
}
