import { test, expect } from "@playwright/test";

test.describe("Visual Regression", () => {
  test("overview page screenshot", async ({ page }) => {
    await page.goto("/");
    await page.waitForTimeout(1000);
    await expect(page).toHaveScreenshot("overview.png", {
      maxDiffPixelRatio: 0.01,
    });
  });

  test("chat page screenshot", async ({ page }) => {
    await page.goto("/#chat");
    await page.waitForTimeout(1000);
    await expect(page).toHaveScreenshot("chat.png", {
      maxDiffPixelRatio: 0.01,
    });
  });

  test("catalog page screenshot", async ({ page }) => {
    await page.goto("/catalog.html");
    await page.waitForTimeout(1000);
    await expect(page).toHaveScreenshot("catalog.png", {
      maxDiffPixelRatio: 0.02,
    });
  });
});
