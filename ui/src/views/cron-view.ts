import { LitElement, html, css } from "lit";
import { customElement, state } from "lit/decorators.js";
import type { GatewayClient } from "../gateway.js";

interface CronJob {
  id?: string;
  expression: string;
  description?: string;
  lastRun?: string;
  status?: string;
}

@customElement("sc-cron-view")
export class ScCronView extends LitElement {
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
    }
    h2 {
      margin: 0;
      font-size: 1.25rem;
      font-weight: 600;
      color: var(--sc-text);
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
    .btn-secondary {
      background: var(--sc-bg-elevated);
      color: var(--sc-text);
    }
    .btn-secondary:hover {
      background: var(--sc-border);
    }
    .btn-danger {
      background: #991b1b;
      color: white;
    }
    .btn-danger:hover {
      background: #b91c1c;
    }
    .job-list {
      display: flex;
      flex-direction: column;
      gap: 0.75rem;
    }
    .job-card {
      padding: 1rem;
      background: var(--sc-bg-surface);
      border: 1px solid var(--sc-border);
      border-radius: var(--sc-radius);
      display: flex;
      align-items: flex-start;
      justify-content: space-between;
      gap: 1rem;
    }
    .job-info {
      flex: 1;
      min-width: 0;
    }
    .job-expression {
      font-family: var(--sc-font-mono);
      font-size: 0.875rem;
      color: var(--sc-accent);
      margin-bottom: 0.25rem;
    }
    .job-description {
      font-size: 0.875rem;
      color: var(--sc-text-muted);
      margin-bottom: 0.25rem;
    }
    .job-meta {
      font-size: 0.75rem;
      color: var(--sc-text-muted);
    }
    .job-actions {
      display: flex;
      gap: 0.5rem;
      flex-shrink: 0;
    }
    .form-overlay {
      position: fixed;
      inset: 0;
      background: rgba(0, 0, 0, 0.6);
      display: flex;
      align-items: center;
      justify-content: center;
      z-index: 1000;
    }
    .form-card {
      padding: 1.5rem;
      background: var(--sc-bg-surface);
      border: 1px solid var(--sc-border);
      border-radius: var(--sc-radius);
      width: 100%;
      max-width: 400px;
    }
    .form-title {
      margin: 0 0 1rem;
      font-size: 1rem;
    }
    .form-group {
      margin-bottom: 1rem;
    }
    .form-group label {
      display: block;
      font-size: 0.75rem;
      color: var(--sc-text-muted);
      margin-bottom: 0.25rem;
    }
    .form-group input {
      width: 100%;
      padding: 0.5rem;
      background: var(--sc-bg);
      border: 1px solid var(--sc-border);
      border-radius: var(--sc-radius);
      color: var(--sc-text);
      font-size: 0.875rem;
      box-sizing: border-box;
    }
    .form-actions {
      display: flex;
      gap: 0.5rem;
      justify-content: flex-end;
      margin-top: 1rem;
    }
    .error {
      color: #f87171;
      font-size: 0.875rem;
      margin-top: 0.5rem;
    }
  `;

  @state() private jobs: CronJob[] = [];
  @state() private loading = false;
  @state() private error = "";
  @state() private showForm = false;
  @state() private formExpression = "";
  @state() private formDescription = "";

  private get gateway(): GatewayClient | null {
    return (
      (document.querySelector("sc-app") as { gateway?: GatewayClient })
        ?.gateway ?? null
    );
  }

  override connectedCallback(): void {
    super.connectedCallback();
    this.loadJobs();
  }

  private async loadJobs(): Promise<void> {
    const gw = this.gateway;
    if (!gw) return;
    this.loading = true;
    this.error = "";
    try {
      const res = (await gw.request<{ jobs?: CronJob[] }>("cron.list", {})) as
        | { jobs?: CronJob[] }
        | { result?: { jobs?: CronJob[] } };
      const jobs =
        (res && "jobs" in res && res.jobs) ||
        (res && "result" in res && res.result?.jobs) ||
        [];
      this.jobs = Array.isArray(jobs) ? jobs : [];
    } catch (e) {
      this.error = e instanceof Error ? e.message : "Failed to load jobs";
      this.jobs = [];
    } finally {
      this.loading = false;
    }
  }

  private openAddForm(): void {
    this.formExpression = "";
    this.formDescription = "";
    this.showForm = true;
  }

  private closeForm(): void {
    this.showForm = false;
  }

  private async submitAdd(): Promise<void> {
    const gw = this.gateway;
    if (!gw || !this.formExpression.trim()) return;
    this.error = "";
    try {
      await gw.request("cron.add", {
        expression: this.formExpression.trim(),
        description: this.formDescription.trim() || undefined,
      });
      this.closeForm();
      await this.loadJobs();
    } catch (e) {
      this.error = e instanceof Error ? e.message : "Failed to add job";
    }
  }

  private async runJob(job: CronJob): Promise<void> {
    const gw = this.gateway;
    if (!gw) return;
    try {
      await gw.request("cron.run", { id: job.id ?? job.expression });
      await this.loadJobs();
    } catch (e) {
      this.error = e instanceof Error ? e.message : "Failed to run job";
    }
  }

  private async deleteJob(job: CronJob): Promise<void> {
    const gw = this.gateway;
    if (!gw) return;
    try {
      await gw.request("cron.remove", { id: job.id ?? job.expression });
      await this.loadJobs();
    } catch (e) {
      this.error = e instanceof Error ? e.message : "Failed to delete job";
    }
  }

  override render() {
    return html`
      <div class="header">
        <h2>Cron Jobs</h2>
        <button class="btn" @click=${this.openAddForm}>Add Job</button>
      </div>
      ${this.error ? html`<p class="error">${this.error}</p>` : ""}
      ${this.loading
        ? html`<p style="color: var(--sc-text-muted)">Loading...</p>`
        : this.jobs.length === 0
          ? html`<p style="color: var(--sc-text-muted)">No cron jobs.</p>`
          : html`
              <div class="job-list">
                ${this.jobs.map(
                  (job) => html`
                    <div class="job-card">
                      <div class="job-info">
                        <div class="job-expression">${job.expression}</div>
                        ${job.description
                          ? html`<div class="job-description">
                              ${job.description}
                            </div>`
                          : ""}
                        <div class="job-meta">
                          ${job.lastRun ? `Last run: ${job.lastRun}` : ""}
                          ${job.status ? ` · ${job.status}` : ""}
                        </div>
                      </div>
                      <div class="job-actions">
                        <button
                          class="btn btn-secondary"
                          @click=${() => this.runJob(job)}
                        >
                          Run Now
                        </button>
                        <button
                          class="btn btn-danger"
                          @click=${() => this.deleteJob(job)}
                        >
                          Delete
                        </button>
                      </div>
                    </div>
                  `,
                )}
              </div>
            `}
      ${this.showForm
        ? html`
            <div class="form-overlay" @click=${this.closeForm}>
              <div
                class="form-card"
                @click=${(e: Event) => e.stopPropagation()}
              >
                <h3 class="form-title">Add Cron Job</h3>
                <div class="form-group">
                  <label>Expression (e.g. 0 * * * *)</label>
                  <input
                    type="text"
                    placeholder="0 * * * *"
                    .value=${this.formExpression}
                    @input=${(e: Event) =>
                      (this.formExpression = (
                        e.target as HTMLInputElement
                      ).value)}
                  />
                </div>
                <div class="form-group">
                  <label>Description</label>
                  <input
                    type="text"
                    placeholder="Optional"
                    .value=${this.formDescription}
                    @input=${(e: Event) =>
                      (this.formDescription = (
                        e.target as HTMLInputElement
                      ).value)}
                  />
                </div>
                <div class="form-actions">
                  <button class="btn btn-secondary" @click=${this.closeForm}>
                    Cancel
                  </button>
                  <button class="btn" @click=${this.submitAdd}>Add</button>
                </div>
              </div>
            </div>
          `
        : ""}
    `;
  }
}
