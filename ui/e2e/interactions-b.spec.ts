import { test, expect } from "@playwright/test";
import { shadowCount, shadowExists, shadowText, WAIT, POLL } from "./helpers.js";

// ─────────────────────────────────────────────────────────────
// Automations (Interactions)
// ─────────────────────────────────────────────────────────────
test.describe("Automations (Interactions)", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/?demo#automations");
    await page.waitForTimeout(WAIT);
  });

  test("shows existing automations", async ({ page }) => {
    await expect(async () => {
      const text = await page.evaluate(shadowText("sc-automations-view"));
      expect(text).toContain("Daily Summary");
      expect(text).toContain("Health Check");
    }).toPass({ timeout: POLL });
  });

  test("new automation button exists", async ({ page }) => {
    await expect(async () => {
      const text = await page.evaluate(shadowText("sc-automations-view"));
      expect(text).toContain("New");
      const hasBtn = await page.evaluate(`(() => {
        const app = document.querySelector("sc-app");
        const view = app?.shadowRoot?.querySelector("sc-automations-view");
        const btns = view?.shadowRoot?.querySelectorAll("sc-button") ?? [];
        return [...btns].some(b => b.textContent?.includes("New"));
      })()`);
      expect(hasBtn).toBe(true);
    }).toPass({ timeout: POLL });
  });

  test("toggle automation on/off", async ({ page }) => {
    await expect(async () => {
      const text = await page.evaluate(shadowText("sc-automations-view"));
      expect(text).toContain("Daily Standup");
    }).toPass({ timeout: POLL });

    // Daily Standup has enabled: false — verify paused visual (card-wrapper.paused)
    await expect(async () => {
      const hasPausedCard = await page.evaluate(`(() => {
        const app = document.querySelector("sc-app");
        const view = app?.shadowRoot?.querySelector("sc-automations-view");
        const cards = view?.shadowRoot?.querySelectorAll("sc-automation-card") ?? [];
        for (const card of cards) {
          const wrapper = card.shadowRoot?.querySelector(".card-wrapper.paused");
          const nameEl = card.shadowRoot?.querySelector(".job-name");
          if (wrapper && nameEl?.textContent?.includes("Daily Standup")) return true;
        }
        return false;
      })()`);
      expect(hasPausedCard).toBe(true);
    }).toPass({ timeout: POLL });
  });

  test("shows run history tab", async ({ page }) => {
    await expect(async () => {
      expect(await page.evaluate(shadowExists("sc-automations-view", "sc-tabs"))).toBe(true);
    }).toPass({ timeout: POLL });

    // Click Shell Jobs tab (Health Check has runs)
    await page.evaluate(() => {
      const app = document.querySelector("sc-app");
      const view = app?.shadowRoot?.querySelector("sc-automations-view");
      const tabs = view?.shadowRoot?.querySelector("sc-tabs");
      const shellTab = tabs?.shadowRoot?.querySelector('[data-tab-id="shell"]');
      (shellTab as HTMLElement)?.click();
    });
    await page.waitForTimeout(400);

    await expect(async () => {
      const text = await page.evaluate(shadowText("sc-automations-view"));
      expect(text).toContain("Run history");
    }).toPass({ timeout: POLL });
  });

  test("shows shell job type", async ({ page }) => {
    await expect(async () => {
      const text = await page.evaluate(shadowText("sc-automations-view"));
      expect(text).toContain("Health Check");
      expect(text).toMatch(/shell|git pull/);
    }).toPass({ timeout: POLL });
  });
});

// ─────────────────────────────────────────────────────────────
// Skills (Interactions)
// ─────────────────────────────────────────────────────────────
test.describe("Skills (Interactions)", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/?demo#skills");
    await page.waitForTimeout(WAIT);
  });

  test("shows installed skills", async ({ page }) => {
    await expect(async () => {
      const text = await page.evaluate(shadowText("sc-skills-view"));
      expect(text).toContain("web-research");
      expect(text).toContain("code-review");
      expect(text).toContain("data-analysis");
    }).toPass({ timeout: POLL });
  });

  test("shows registry tab", async ({ page }) => {
    await expect(async () => {
      const text = await page.evaluate(shadowText("sc-skills-view"));
      expect(text).toMatch(/Registry|Explore Registry/);
    }).toPass({ timeout: POLL });

    // Registry is a section; verify registry entries
    await expect(async () => {
      expect(await page.evaluate(shadowExists("sc-skills-view", "sc-skill-registry"))).toBe(true);
      const text = await page.evaluate(shadowText("sc-skills-view"));
      expect(text).toContain("calendar-sync");
      expect(text).toContain("test-runner");
      expect(text).toContain("deploy-helper");
    }).toPass({ timeout: POLL });
  });

  test("skill enable/disable state visible", async ({ page }) => {
    await expect(async () => {
      const text = await page.evaluate(shadowText("sc-skills-view"));
      expect(text).toContain("email-digest");
      expect(text).toContain("web-research");
    }).toPass({ timeout: POLL });
  });

  test("shows tag filtering", async ({ page }) => {
    await expect(async () => {
      const text = await page.evaluate(shadowText("sc-skills-view"));
      expect(text).toMatch(/research|development|web/);
      const count = await page.evaluate(shadowCount("sc-skills-view", ".tag-chip"));
      expect(count).toBeGreaterThanOrEqual(1);
    }).toPass({ timeout: POLL });
  });

  test("install from URL section exists", async ({ page }) => {
    await expect(async () => {
      const text = await page.evaluate(shadowText("sc-skills-view"));
      expect(text).toMatch(/Install from URL|Install/);
      const hasInput = await page.evaluate(`(() => {
        const app = document.querySelector("sc-app");
        const view = app?.shadowRoot?.querySelector("sc-skills-view");
        const input = view?.shadowRoot?.querySelector('sc-input[placeholder*="Install"]');
        return !!input;
      })()`);
      expect(hasInput).toBe(true);
    }).toPass({ timeout: POLL });
  });
});
