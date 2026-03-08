import { test, expect } from "@playwright/test";
import * as fs from "node:fs";
import { waitForViewReady } from "./helpers.js";

const VIEWS = [
  "overview",
  "chat",
  "agents",
  "models",
  "tools",
  "channels",
  "skills",
  "automations",
  "config",
  "voice",
  "nodes",
  "usage",
  "security",
  "logs",
];

test.describe("Screenshot All Views (Demo Mode)", () => {
  test.beforeAll(() => {
    const dir = "e2e-screenshots";
    if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });
  });

  for (const view of VIEWS) {
    test(`screenshot ${view}`, async ({ page }) => {
      const hash = view === "overview" ? "" : `#${view}`;
      await page.goto(`/?demo${hash}`);
      const viewTag = view === "overview" ? "sc-overview-view" : `sc-${view}-view`;
      await waitForViewReady(page, viewTag);
      await page.screenshot({
        path: `e2e-screenshots/${view}.png`,
        fullPage: true,
      });
    });
  }
});
