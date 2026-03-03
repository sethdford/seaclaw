import { test, expect } from "@playwright/test";

test.describe("SeaClaw Control UI", () => {
  test("loads and shows the page title", async ({ page }) => {
    await page.goto("/");
    await expect(page).toHaveTitle(/SeaClaw Control/);
  });

  test("app shell renders with custom element", async ({ page }) => {
    await page.goto("/");
    await expect(page.locator("sc-app")).toBeAttached();
  });

  test("sidebar renders inside app shell", async ({ page }) => {
    await page.goto("/");
    const sidebar = page.locator("sc-app >> sc-sidebar");
    await expect(sidebar).toBeAttached({ timeout: 5000 });
  });

  test("overview is the default view", async ({ page }) => {
    await page.goto("/");
    const overview = page.locator("sc-app >> sc-overview-view");
    await expect(overview).toBeAttached({ timeout: 5000 });
  });

  test("hash navigation loads chat view", async ({ page }) => {
    await page.goto("/#chat");
    await page.waitForTimeout(500);
    const chatView = page.locator("sc-app >> sc-chat-view");
    await expect(chatView).toBeAttached({ timeout: 5000 });
  });

  test("hash navigation loads models view", async ({ page }) => {
    await page.goto("/#models");
    await page.waitForTimeout(500);
    const modelsView = page.locator("sc-app >> sc-models-view");
    await expect(modelsView).toBeAttached({ timeout: 5000 });
  });

  test("hash navigation loads tools view", async ({ page }) => {
    await page.goto("/#tools");
    await page.waitForTimeout(500);
    const toolsView = page.locator("sc-app >> sc-tools-view");
    await expect(toolsView).toBeAttached({ timeout: 5000 });
  });

  test("hash navigation loads config view", async ({ page }) => {
    await page.goto("/#config");
    await page.waitForTimeout(500);
    const configView = page.locator("sc-app >> sc-config-view");
    await expect(configView).toBeAttached({ timeout: 5000 });
  });

  test("hash navigation loads logs view", async ({ page }) => {
    await page.goto("/#logs");
    await page.waitForTimeout(500);
    const logsView = page.locator("sc-app >> sc-logs-view");
    await expect(logsView).toBeAttached({ timeout: 5000 });
  });

  test("invalid hash falls back to overview", async ({ page }) => {
    await page.goto("/#nonexistent");
    await page.waitForTimeout(500);
    const overview = page.locator("sc-app >> sc-overview-view");
    await expect(overview).toBeAttached({ timeout: 5000 });
  });
});
