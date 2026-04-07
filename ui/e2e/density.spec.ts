import { test, expect } from "@playwright/test";
import * as fs from "node:fs";
import * as path from "node:path";
import { waitForViewReady } from "./helpers.js";

/**
 * Multi-breakpoint density E2E tests — captures viewport screenshots at all 4
 * breakpoints (compact, medium, expanded, wide) for visual regression and
 * information density validation per quality-scorecard.md.
 *
 * Skip behavior: Tests are skipped in non-CI environments when no baseline
 * exists for the current platform (browser + OS). CI runs these with
 * `--update-snapshots` to generate baselines; local runs need
 * `npx playwright test density.spec.ts --update-snapshots` to create them.
 */

function snapshotExists(testInfo: { snapshotDir: string }, name: string): boolean {
  const snapshotPath = path.join(testInfo.snapshotDir, name);
  return fs.existsSync(snapshotPath);
}

const BREAKPOINTS = [
  { name: "compact", width: 375, height: 812 },
  { name: "medium", width: 768, height: 1024 },
  { name: "expanded", width: 1024, height: 768 },
  { name: "wide", width: 1440, height: 900 },
];

for (const bp of BREAKPOINTS) {
  test(`dashboard at ${bp.name} (${bp.width}px)`, async ({ page }, testInfo) => {
    await page.setViewportSize({ width: bp.width, height: bp.height });
    await page.goto("/?demo#overview");
    await page.waitForLoadState("networkidle");
    await waitForViewReady(page, "hu-overview-view", 15000);
    const snapName = `density-${bp.name}-${testInfo.project.name}-${process.platform}.png`;
    const updating = testInfo.config.updateSnapshots !== "none";
    // Skipped in non-CI when no baseline exists; CI generates with --update-snapshots
    if (!updating && !snapshotExists(testInfo, snapName)) {
      test.skip(
        true,
        `No baseline for ${process.platform}; run: npx playwright test density.spec.ts --update-snapshots`,
      );
    }
    await expect(page).toHaveScreenshot(`density-${bp.name}.png`, {
      maxDiffPixelRatio: 0.02,
    });
  });
}
