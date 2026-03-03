import { html, css } from "lit";
import { customElement, state } from "lit/decorators.js";
import { GatewayAwareLitElement } from "../gateway-aware.js";
import "../components/sc-card.js";
import "../components/sc-empty-state.js";

interface PolicySummary {
  sandbox_enabled?: boolean;
  sandbox_backend?: string;
  allowed_commands?: string[];
  pairing_required?: boolean;
  https_only?: boolean;
}

@customElement("sc-security-view")
export class ScSecurityView extends GatewayAwareLitElement {
  static override styles = css`
    :host {
      display: block;
      color: var(--sc-text);
    }
    h2 {
      margin: 0 0 var(--sc-space-md);
      font-size: var(--sc-text-xl);
      font-weight: var(--sc-weight-semibold);
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
      gap: var(--sc-space-md);
    }
    .policy-item {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: var(--sc-space-sm) 0;
      border-bottom: 1px solid var(--sc-border-subtle);
    }
    .policy-item:last-child {
      border-bottom: none;
    }
    .label {
      font-size: var(--sc-text-sm);
      color: var(--sc-text-muted);
    }
    .value {
      font-size: var(--sc-text-sm);
      font-weight: var(--sc-weight-medium);
    }
    .badge-on {
      color: var(--sc-success);
    }
    .badge-off {
      color: var(--sc-text-faint);
    }
    .commands {
      display: flex;
      flex-wrap: wrap;
      gap: var(--sc-space-xs);
      margin-top: var(--sc-space-xs);
    }
    .cmd {
      font-family: var(--sc-font-mono);
      font-size: var(--sc-text-xs);
      background: var(--sc-bg-elevated);
      padding: 2px var(--sc-space-xs);
      border-radius: var(--sc-radius-sm);
    }
  `;

  @state() private _policy: PolicySummary | null = null;
  @state() private _loading = true;

  override connectedCallback() {
    super.connectedCallback();
    this._fetchPolicy();
  }

  private async _fetchPolicy() {
    try {
      const data = await this._gw?.request("security.policy", {});
      if (data) this._policy = data as PolicySummary;
    } catch {
      /* gateway may not support this endpoint yet */
    }
    this._loading = false;
  }

  override render() {
    if (this._loading) {
      return html`<h2>Security</h2>
        <sc-skeleton variant="card"></sc-skeleton>`;
    }
    if (!this._policy) {
      return html`
        <h2>Security</h2>
        <sc-empty-state
          icon="shield"
          title="Security Policy"
          description="Connect to a SeaClaw gateway to view security settings."
        ></sc-empty-state>
      `;
    }
    const p = this._policy;
    return html`
      <h2>Security</h2>
      <div class="grid">
        <sc-card>
          <div class="policy-item">
            <span class="label">Sandbox</span>
            <span class="value ${p.sandbox_enabled ? "badge-on" : "badge-off"}">
              ${p.sandbox_enabled ? "Enabled" : "Disabled"}
            </span>
          </div>
          ${p.sandbox_backend
            ? html`<div class="policy-item">
                <span class="label">Backend</span>
                <span class="value">${p.sandbox_backend}</span>
              </div>`
            : ""}
          <div class="policy-item">
            <span class="label">Pairing Required</span>
            <span class="value ${p.pairing_required ? "badge-on" : "badge-off"}">
              ${p.pairing_required ? "Yes" : "No"}
            </span>
          </div>
          <div class="policy-item">
            <span class="label">HTTPS Only</span>
            <span class="value ${p.https_only ? "badge-on" : "badge-off"}">
              ${p.https_only ? "Yes" : "No"}
            </span>
          </div>
        </sc-card>
        ${p.allowed_commands?.length
          ? html`<sc-card>
              <div class="label">Allowed Commands</div>
              <div class="commands">
                ${p.allowed_commands.map((c) => html`<span class="cmd">${c}</span>`)}
              </div>
            </sc-card>`
          : ""}
      </div>
    `;
  }
}
