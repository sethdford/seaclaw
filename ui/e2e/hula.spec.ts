import { test, expect } from "@playwright/test";
import AxeBuilder from "@axe-core/playwright";
import { deepText, shadowExists, waitForViewReady, POLL } from "./helpers.js";

/** All axe rules run with zero exclusions (matches accessibility.spec.ts). */
const SHADOW_DOM_EXCLUDED_RULES: string[] = [];

test.describe("HuLa (Demo)", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/?demo#hula");
    await waitForViewReady(page, "hu-hula-view");
  });

  test("hu-app root is visible", async ({ page }) => {
    await expect(page.locator("hu-app")).toBeVisible();
  });

  test("view renders with substantive content (not blank)", async ({ page }) => {
    await expect(async () => {
      const text = await page.evaluate(deepText("hu-hula-view"));
      expect(text.trim().length).toBeGreaterThan(20);
    }).toPass({ timeout: POLL });
  });

  test("passes axe accessibility (critical and serious)", async ({ page }) => {
    await page.waitForLoadState("domcontentloaded");
    const results = await new AxeBuilder({ page })
      .withTags(["wcag2a", "wcag2aa", "wcag21aa"])
      .disableRules(SHADOW_DOM_EXCLUDED_RULES)
      .analyze();
    const critical = results.violations.filter(
      (v) => v.impact === "critical" || v.impact === "serious",
    );
    expect(critical).toEqual([]);
  });

  test("shows page hero and HuLa heading", async ({ page }) => {
    await expect(async () => {
      expect(await page.evaluate(shadowExists("hu-hula-view", "hu-page-hero"))).toBe(true);
      const text = await page.evaluate(deepText("hu-hula-view"));
      expect(text).toContain("HuLa");
    }).toPass({ timeout: POLL });
  });
});
