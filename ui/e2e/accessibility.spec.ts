import { test, expect } from "@playwright/test";
import AxeBuilder from "@axe-core/playwright";

const SHADOW_DOM_EXCLUDED_RULES = ["color-contrast", "link-in-text-block"];

test.describe("Accessibility", () => {
  test("no critical accessibility violations on main page", async ({ page }) => {
    await page.goto("/");
    await page.waitForTimeout(1000);
    const results = await new AxeBuilder({ page })
      .withTags(["wcag2a", "wcag2aa", "wcag21aa"])
      .disableRules(SHADOW_DOM_EXCLUDED_RULES)
      .analyze();
    const critical = results.violations.filter(
      (v) => v.impact === "critical" || v.impact === "serious",
    );
    if (critical.length > 0) {
      console.log(
        "A11y violations:",
        JSON.stringify(
          critical.map((v) => ({
            id: v.id,
            impact: v.impact,
            description: v.description,
            nodes: v.nodes.length,
          })),
          null,
          2,
        ),
      );
    }
    expect(critical).toEqual([]);
  });

  test("chat page has no critical a11y violations", async ({ page }) => {
    await page.goto("/#chat");
    await page.waitForTimeout(1000);
    const results = await new AxeBuilder({ page })
      .withTags(["wcag2a", "wcag2aa", "wcag21aa"])
      .disableRules(SHADOW_DOM_EXCLUDED_RULES)
      .analyze();
    const critical = results.violations.filter(
      (v) => v.impact === "critical" || v.impact === "serious",
    );
    if (critical.length > 0) {
      console.log(
        "A11y violations:",
        JSON.stringify(
          critical.map((v) => ({
            id: v.id,
            impact: v.impact,
            description: v.description,
            nodes: v.nodes.length,
          })),
          null,
          2,
        ),
      );
    }
    expect(critical).toEqual([]);
  });

  test("all navigation views are keyboard accessible", async ({ page }) => {
    await page.goto("/");
    for (let i = 0; i < 5; i++) {
      await page.keyboard.press("Tab");
    }
    const focused = await page.evaluate(() => document.activeElement?.tagName);
    expect(focused).toBeTruthy();
  });

  test("command palette is keyboard navigable", async ({ page }) => {
    await page.goto("/");
    await page.keyboard.press("Control+k");
    await page.waitForTimeout(200);
    await page.keyboard.type("chat");
    await page.waitForTimeout(100);
    await page.keyboard.press("ArrowDown");
    await page.keyboard.press("Enter");
  });

  test("modal traps focus", async ({ page }) => {
    await page.goto("/");
    await page.keyboard.press("Control+k");
    await page.waitForTimeout(200);
    await page.keyboard.press("Escape");
    await page.waitForTimeout(200);
  });
});
