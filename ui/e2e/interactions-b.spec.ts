import { test, expect } from "@playwright/test";
import {
  shadowCount,
  shadowExists,
  shadowText,
  deepText,
  WAIT,
  POLL,
} from "./helpers.js";

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
      const text: string = await page.evaluate(deepText("sc-automations-view"));
      expect(text).toContain("Daily Summary");
    }).toPass({ timeout: POLL });
  });

  test("new automation button exists", async ({ page }) => {
    await expect(async () => {
      const hasBtn = await page.evaluate(`(() => {
        const app = document.querySelector("sc-app");
        const view = app?.shadowRoot?.querySelector("sc-automations-view");
        const btns = view?.shadowRoot?.querySelectorAll("sc-button") ?? [];
        return [...btns].some(b => b.textContent?.includes("New"));
      })()`);
      expect(hasBtn).toBe(true);
    }).toPass({ timeout: POLL });
  });

  test("paused automation shows paused state", async ({ page }) => {
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

  test("shell jobs tab shows shell automations", async ({ page }) => {
    await expect(async () => {
      expect(await page.evaluate(shadowExists("sc-automations-view", "sc-tabs"))).toBe(true);
    }).toPass({ timeout: POLL });

    await page.evaluate(() => {
      const app = document.querySelector("sc-app");
      const view = app?.shadowRoot?.querySelector("sc-automations-view");
      const tabs = view?.shadowRoot?.querySelector("sc-tabs");
      const shellTab = tabs?.shadowRoot?.querySelector('[data-tab-id="shell"]') as HTMLElement | null;
      shellTab?.click();
    });
    await page.waitForTimeout(600);

    await expect(async () => {
      const text: string = await page.evaluate(deepText("sc-automations-view"));
      expect(text).toContain("Health Check");
    }).toPass({ timeout: POLL });
  });

  test("automation card shows type badge", async ({ page }) => {
    await expect(async () => {
      const text: string = await page.evaluate(deepText("sc-automations-view"));
      expect(text).toMatch(/Agent|Shell/);
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
      const count = await page.evaluate(shadowCount("sc-skills-view", "sc-skill-card"));
      expect(count).toBeGreaterThanOrEqual(3);
    }).toPass({ timeout: POLL });
  });

  test("shows registry section with entries", async ({ page }) => {
    await expect(async () => {
      expect(await page.evaluate(shadowExists("sc-skills-view", "sc-skill-registry"))).toBe(true);
      const text: string = await page.evaluate(deepText("sc-skills-view"));
      expect(text).toMatch(/calendar-sync|test-runner|deploy-helper/);
    }).toPass({ timeout: POLL });
  });

  test("skill enable/disable state visible", async ({ page }) => {
    await expect(async () => {
      const text: string = await page.evaluate(deepText("sc-skills-view"));
      expect(text).toMatch(/Enabled|Active/);
      expect(text).toMatch(/Disabled|Inactive/);
    }).toPass({ timeout: POLL });
  });

  test("shows tags in skill cards", async ({ page }) => {
    await expect(async () => {
      const text: string = await page.evaluate(deepText("sc-skills-view"));
      expect(text).toMatch(/research|development|web/);
    }).toPass({ timeout: POLL });
  });

  test("install from URL section exists", async ({ page }) => {
    await expect(async () => {
      const text: string = await page.evaluate(deepText("sc-skills-view"));
      expect(text).toMatch(/Install|URL/i);
    }).toPass({ timeout: POLL });
  });
});
