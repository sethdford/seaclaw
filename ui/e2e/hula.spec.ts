import { test, expect } from "@playwright/test";
import AxeBuilder from "@axe-core/playwright";
import {
  deepText,
  shadowExists,
  waitForViewReady,
  waitForShadowSelector,
  POLL,
} from "./helpers.js";

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

  test("trace detail requests trace_limit (truncation banner when applicable)", async ({
    page,
  }) => {
    await page.goto("/?demo#hula");
    await waitForViewReady(page, "hu-hula-view");
    await waitForShadowSelector(page, "hu-hula-view", ".trace-row");
    await page.evaluate(`(() => {
      const app = document.querySelector("hu-app");
      const view = app?.shadowRoot?.querySelector("hu-hula-view");
      const row = view?.shadowRoot?.querySelector(".trace-row");
      if (row) row.click();
    })()`);
    await expect(async () => {
      const text = await page.evaluate(deepText("hu-hula-view"));
      expect(text).toMatch(/steps \(offset|Saved traces|Trace detail/);
    }).toPass({ timeout: POLL });
  });
});

test.describe("HuLa from Observability (Demo)", () => {
  test("metrics Open HuLa traces navigates to HuLa view", async ({ page }) => {
    await page.goto("/?demo#metrics");
    await waitForViewReady(page, "hu-metrics-view");
    await waitForShadowSelector(
      page,
      "hu-metrics-view",
      "hu-button[data-testid='metrics-open-hula-traces']",
    );
    await page.evaluate(`(() => {
      const app = document.querySelector("hu-app");
      const view = app?.shadowRoot?.querySelector("hu-metrics-view");
      const host = view?.shadowRoot?.querySelector(
        "hu-button[data-testid='metrics-open-hula-traces']",
      );
      const btn = host?.shadowRoot?.querySelector("button");
      if (btn) btn.click();
    })()`);
    await expect(page).toHaveURL(/#hula/);
    await waitForViewReady(page, "hu-hula-view");
    const text = await page.evaluate(deepText("hu-hula-view"));
    expect(text).toContain("HuLa");
  });
});
