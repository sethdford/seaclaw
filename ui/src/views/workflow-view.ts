import { html, css } from "lit";
import { customElement } from "lit/decorators.js";
import { GatewayAwareLitElement } from "../gateway-aware.js";
import "../components/hu-page-hero.js";
import "../components/hu-card.js";

/**
 * hu-workflow-view
 *
 * Displays workflow and automation execution history, status, and logs.
 * This view allows users to monitor and manage running workflows.
 */
@customElement("hu-workflow-view")
export class HuWorkflowView extends GatewayAwareLitElement {
  static override styles = css`
    :host {
      display: block;
    }

    .content {
      padding: var(--hu-space-lg);
    }

    .workflow-section {
      display: grid;
      gap: var(--hu-space-lg);
    }

    hu-card {
      background: var(--hu-bg-surface);
      border: 1px solid var(--hu-border);
    }
  `;

  override render() {
    return html`
      <div class="content">
        <hu-page-hero
          title="Workflows"
          subtitle="Monitor and manage automation workflows"
        ></hu-page-hero>

        <div class="workflow-section">
          <hu-card>
            <div style="padding: var(--hu-space-lg);">
              <h3>Workflow Dashboard</h3>
              <p>Workflow execution tracking and automation status coming soon.</p>
            </div>
          </hu-card>
        </div>
      </div>
    `;
  }
}
