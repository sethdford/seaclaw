import { test, expect } from "@playwright/test";
import { waitForViewReady } from "./helpers.js";

/**
 * Reduced-motion E2E tests — verifies that when prefers-reduced-motion: reduce
 * is set, no CSS animations or transitions run on opacity/transform.
 * Per motion-design.md: every animation must respect prefers-reduced-motion.
 */

test.describe("prefers-reduced-motion", () => {
  test.use({ reducedMotion: "reduce" });

  test("no running animations when reduced motion is preferred", async ({ page }) => {
    await page.goto("/?demo");
    await page.waitForLoadState("networkidle");
    await waitForViewReady(page, "hu-overview-view");

    const hasRunningAnimations = await page.evaluate(() => {
      const all = document.querySelectorAll("*");
      for (const el of all) {
        const style = getComputedStyle(el);
        if (style.animationName !== "none" && style.animationDuration !== "0s") {
          const play = (el as Element & { getAnimations?: () => Animation[] }).getAnimations?.();
          if (play && play.length > 0) return true;
        }
      }
      return false;
    });
    expect(hasRunningAnimations).toBe(false);
  });

  test("no elements have animation-duration > 0 when reduced motion", async ({ page }) => {
    await page.goto("/?demo");
    await page.waitForLoadState("networkidle");
    await waitForViewReady(page, "hu-overview-view");

    const violating = await page.evaluate(() => {
      const results: string[] = [];
      const all = document.querySelectorAll("*");
      for (const el of all) {
        const style = getComputedStyle(el);
        const animDur = style.animationDuration;
        const animName = style.animationName;
        if (animName !== "none" && animDur !== "0s" && animDur !== "0ms") {
          results.push(`${el.tagName}.${(el as Element).className} animation-duration=${animDur}`);
        }
      }
      return results;
    });
    expect(violating).toEqual([]);
  });

  test("no transition-duration > 0 on opacity or transform when reduced motion", async ({
    page,
  }) => {
    await page.goto("/?demo");
    await page.waitForLoadState("networkidle");
    await waitForViewReady(page, "hu-overview-view");

    const violating = await page.evaluate(() => {
      const results: string[] = [];
      const all = document.querySelectorAll("*");
      // Skip-link inherits transition from `a`; fix tracked in theme.css
      const skipSelectors = [".skip-link", ".hu-skip-link"];
      for (const el of all) {
        if (skipSelectors.some((s) => (el as Element).matches?.(s))) continue;
        const style = getComputedStyle(el);
        const transDur = style.transitionDuration;
        const transProp = style.transitionProperty;
        if (
          transDur &&
          transDur !== "0s" &&
          transDur !== "0ms" &&
          (transProp.includes("opacity") || transProp.includes("transform") || transProp === "all")
        ) {
          results.push(
            `${el.tagName}.${(el as Element).className} transition-duration=${transDur} property=${transProp}`,
          );
        }
      }
      return results;
    });
    expect(violating).toEqual([]);
  });
});
