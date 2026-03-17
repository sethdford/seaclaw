import { test, expect } from "@playwright/test";

/**
 * Responsive layout E2E tests — verifies 4-breakpoint system at compact (375px),
 * medium (768px), and wide (1440px) viewports.
 */

test.describe("Responsive: Compact (375px — iPhone)", () => {
  test.beforeEach(async ({ page }) => {
    await page.setViewportSize({ width: 375, height: 667 });
    await page.goto("/?demo#overview");
    await page.waitForLoadState("domcontentloaded");
  });

  test("bottom nav is visible", async ({ page }) => {
    const mobileNav = page.locator("hu-app >> nav.mobile-nav");
    await expect(mobileNav).toBeAttached({ timeout: 5000 });
    await expect(mobileNav).toBeVisible();
  });

  test("sidebar is hidden", async ({ page }) => {
    const sidebar = page.locator("hu-app >> hu-sidebar");
    await expect(sidebar).toBeAttached({ timeout: 5000 });
    const isHidden = await sidebar.evaluate((el) => {
      const style = window.getComputedStyle(el);
      return style.display === "none";
    });
    expect(isHidden).toBe(true);
  });

  test("main content fills width", async ({ page }) => {
    const main = page.locator("hu-app >> main");
    await expect(main).toBeAttached({ timeout: 5000 });
    const rect = await main.boundingBox();
    expect(rect).toBeTruthy();
    expect(rect!.width).toBeLessThanOrEqual(400);
    expect(rect!.width).toBeGreaterThan(250);
  });

  test("mobile tabs are clickable", async ({ page }) => {
    const chatTab = page.locator('hu-app >> nav.mobile-nav >> button.mobile-tab[aria-label="Chat"]');
    await expect(chatTab).toBeVisible({ timeout: 5000 });
    await chatTab.click();
    await page.waitForLoadState("domcontentloaded");
    const chatView = page.locator("hu-app >> hu-chat-view");
    await expect(chatView).toBeAttached({ timeout: 5000 });
  });
});

test.describe("Responsive: Medium (768px — iPad)", () => {
  test.beforeEach(async ({ page }) => {
    await page.setViewportSize({ width: 768, height: 1024 });
    await page.goto("/?demo#overview");
    await page.waitForLoadState("domcontentloaded");
  });

  test("sidebar is visible and collapsed", async ({ page }) => {
    const sidebar = page.locator("hu-app >> hu-sidebar");
    await expect(sidebar).toBeAttached({ timeout: 5000 });
    const isVisible = await sidebar.evaluate((el) => {
      const style = window.getComputedStyle(el);
      return style.display !== "none";
    });
    expect(isVisible).toBe(true);

    const layout = page.locator("hu-app >> .layout");
    const hasCollapsed = await layout.evaluate((el) => el.classList.contains("collapsed"));
    expect(hasCollapsed).toBe(true);
  });

  test("mobile nav is hidden", async ({ page }) => {
    const mobileNav = page.locator("hu-app >> nav.mobile-nav");
    await expect(mobileNav).toBeAttached({ timeout: 5000 });
    const isHidden = await mobileNav.evaluate((el) => {
      const style = window.getComputedStyle(el);
      return style.display === "none";
    });
    expect(isHidden).toBe(true);
  });

  test("content is responsive", async ({ page }) => {
    const main = page.locator("hu-app >> main");
    await expect(main).toBeAttached({ timeout: 5000 });
    const rect = await main.boundingBox();
    expect(rect).toBeTruthy();
    expect(rect!.width).toBeGreaterThan(400);
    expect(rect!.width).toBeLessThanOrEqual(768);
  });
});

test.describe("Responsive: Wide (1440px)", () => {
  test.beforeEach(async ({ page }) => {
    await page.setViewportSize({ width: 1440, height: 900 });
    await page.goto("/?demo#overview");
    await page.waitForLoadState("domcontentloaded");
  });

  test("full layout with sidebar expanded", async ({ page }) => {
    const sidebar = page.locator("hu-app >> hu-sidebar");
    await expect(sidebar).toBeAttached({ timeout: 5000 });
    const isVisible = await sidebar.evaluate((el) => {
      const style = window.getComputedStyle(el);
      return style.display !== "none";
    });
    expect(isVisible).toBe(true);

    const layout = page.locator("hu-app >> .layout");
    const hasCollapsed = await layout.evaluate((el) => el.classList.contains("collapsed"));
    expect(hasCollapsed).toBe(false);
  });

  test("mobile nav is hidden", async ({ page }) => {
    const mobileNav = page.locator("hu-app >> nav.mobile-nav");
    await expect(mobileNav).toBeAttached({ timeout: 5000 });
    const isHidden = await mobileNav.evaluate((el) => {
      const style = window.getComputedStyle(el);
      return style.display === "none";
    });
    expect(isHidden).toBe(true);
  });

  test("content area has full width", async ({ page }) => {
    const main = page.locator("hu-app >> main");
    await expect(main).toBeAttached({ timeout: 5000 });
    const rect = await main.boundingBox();
    expect(rect).toBeTruthy();
    expect(rect!.width).toBeGreaterThan(400);
  });

  test("list-detail view shows detail panel at wide", async ({ page }) => {
    await page.goto("/?demo#sessions");
    await page.waitForLoadState("domcontentloaded");

    const detailPanel = page.locator("hu-app >> .detail-panel");
    await expect(detailPanel).toBeAttached({ timeout: 5000 });
    const isVisible = await detailPanel.evaluate((el) => {
      const style = window.getComputedStyle(el);
      return style.display === "block";
    });
    expect(isVisible).toBe(true);
  });
});
