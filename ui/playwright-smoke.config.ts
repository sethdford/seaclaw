import { defineConfig, devices } from "@playwright/test";

/**
 * Smoke test config — runs against the live C gateway serving the UI.
 *
 * The gateway must be started before running these tests:
 *   ./build/human gateway  (with control_ui_dir pointing at ui/dist)
 *
 * Usage:
 *   npx playwright test --config playwright-smoke.config.ts
 *   GATEWAY_PORT=3002 npx playwright test --config playwright-smoke.config.ts
 */

const port = process.env.GATEWAY_PORT ?? "3000";

export default defineConfig({
  testDir: "./e2e",
  testMatch: "smoke.spec.ts",
  fullyParallel: false,
  retries: process.env.CI ? 1 : 0,
  workers: 1,
  timeout: 30000,
  reporter: process.env.CI ? "github" : "list",
  use: {
    baseURL: `http://localhost:${port}`,
    trace: "on-first-retry",
    screenshot: "only-on-failure",
    expect: { timeout: 10000 },
  },
  projects: [
    {
      name: "chromium",
      use: { ...devices["Desktop Chrome"] },
    },
  ],
});
