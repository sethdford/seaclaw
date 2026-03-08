import { test, expect } from "@playwright/test";
import { shadowCount, shadowExists, shadowText, deepText, WAIT, POLL } from "./helpers.js";

// ─────────────────────────────────────────────────────────────
// Models (Interactions)
// ─────────────────────────────────────────────────────────────
test.describe("Models (Interactions)", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/?demo#models");
    await page.waitForTimeout(WAIT);
  });

  test("shows provider cards", async ({ page }) => {
    await expect(async () => {
      const count = await page.evaluate(shadowCount("sc-models-view", "sc-card"));
      expect(count).toBeGreaterThanOrEqual(3);
    }).toPass({ timeout: POLL });
  });

  test("default provider is marked", async ({ page }) => {
    await expect(async () => {
      const text: string = await page.evaluate(deepText("sc-models-view"));
      expect(text).toContain("openrouter");
      expect(text).toMatch(/Default|default/);
    }).toPass({ timeout: POLL });
  });

  test("search component exists", async ({ page }) => {
    await expect(async () => {
      expect(await page.evaluate(shadowExists("sc-models-view", "sc-search"))).toBe(true);
    }).toPass({ timeout: POLL });
  });
});

// ─────────────────────────────────────────────────────────────
// Voice (Interactions)
// ─────────────────────────────────────────────────────────────
test.describe("Voice (Interactions)", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/?demo#voice");
    await page.waitForTimeout(WAIT);
  });

  test("mic button exists in orb", async ({ page }) => {
    await expect(async () => {
      expect(await page.evaluate(shadowExists("sc-voice-view", "sc-voice-orb"))).toBe(true);
    }).toPass({ timeout: POLL });
  });

  test("text input and send button exist", async ({ page }) => {
    await expect(async () => {
      const text: string = await page.evaluate(deepText("sc-voice-view"));
      expect(text).toMatch(/Send|send/);
    }).toPass({ timeout: POLL });
  });

  test("new session button exists", async ({ page }) => {
    await expect(async () => {
      const hasBtn = await page.evaluate(`(() => {
        const app = document.querySelector("sc-app");
        const view = app?.shadowRoot?.querySelector("sc-voice-view");
        const btns = view?.shadowRoot?.querySelectorAll("sc-button") ?? [];
        return [...btns].some(b => {
          const label = b.getAttribute("aria-label") ?? "";
          return label.includes("new session") || label.includes("New session") || label.includes("Start new");
        });
      })()`);
      expect(hasBtn).toBe(true);
    }).toPass({ timeout: POLL });
  });
});

// ─────────────────────────────────────────────────────────────
// Overview (Interactions)
// ─────────────────────────────────────────────────────────────
test.describe("Overview (Interactions)", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/?demo");
    await page.evaluate(() => localStorage.setItem("sc-welcomed", "true"));
    await page.goto("/?demo#overview");
    await page.waitForTimeout(WAIT);
  });

  test("quick action cards exist", async ({ page }) => {
    await expect(async () => {
      const text: string = await page.evaluate(deepText("sc-overview-view"));
      expect(text).toMatch(/Chat|Automation|Voice/);
    }).toPass({ timeout: POLL });
  });

  test("session rows are clickable", async ({ page }) => {
    await expect(async () => {
      const hasRow = await page.evaluate(`(() => {
        const app = document.querySelector("sc-app");
        const view = app?.shadowRoot?.querySelector("sc-overview-view");
        const table = view?.shadowRoot?.querySelector("sc-sessions-table");
        if (!table?.shadowRoot) return false;
        const rows = table.shadowRoot.querySelectorAll('[role="button"]');
        return rows.length > 0;
      })()`);
      expect(hasRow).toBe(true);
    }).toPass({ timeout: POLL });
  });

  test("stat cards display metrics", async ({ page }) => {
    await expect(async () => {
      const count = await page.evaluate(`(() => {
        const app = document.querySelector("sc-app");
        const view = app?.shadowRoot?.querySelector("sc-overview-view");
        const stats = view?.shadowRoot?.querySelector("sc-overview-stats");
        if (!stats?.shadowRoot) return 0;
        return stats.shadowRoot.querySelectorAll("sc-stat-card").length;
      })()`);
      expect(count).toBeGreaterThanOrEqual(2);
    }).toPass({ timeout: POLL });
  });
});

// ─────────────────────────────────────────────────────────────
// Agents (Interactions)
// ─────────────────────────────────────────────────────────────
test.describe("Agents (Interactions)", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/?demo#agents");
    await page.waitForTimeout(WAIT);
  });

  test("shows agent data table", async ({ page }) => {
    await expect(async () => {
      expect(await page.evaluate(shadowExists("sc-agents-view", "sc-data-table-v2"))).toBe(true);
    }).toPass({ timeout: POLL });
  });

  test("new chat button exists", async ({ page }) => {
    await expect(async () => {
      const hasNewChat = await page.evaluate(`(() => {
        const app = document.querySelector("sc-app");
        const view = app?.shadowRoot?.querySelector("sc-agents-view");
        const btns = view?.shadowRoot?.querySelectorAll("sc-button") ?? [];
        return [...btns].some(b => {
          const label = b.getAttribute("aria-label") ?? "";
          const text = b.textContent ?? "";
          return label.includes("new conversation") || label.includes("New") || text.includes("New Chat");
        });
      })()`);
      expect(hasNewChat).toBe(true);
    }).toPass({ timeout: POLL });
  });

  test("shows agent stats", async ({ page }) => {
    await expect(async () => {
      const count = await page.evaluate(shadowCount("sc-agents-view", "sc-stat-card"));
      expect(count).toBeGreaterThanOrEqual(2);
    }).toPass({ timeout: POLL });
  });
});

// ─────────────────────────────────────────────────────────────
// Channels (Interactions)
// ─────────────────────────────────────────────────────────────
test.describe("Channels (Interactions)", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/?demo#channels");
    await page.waitForTimeout(WAIT);
  });

  test("shows channel data table", async ({ page }) => {
    await expect(async () => {
      expect(await page.evaluate(shadowExists("sc-channels-view", "sc-data-table-v2"))).toBe(true);
    }).toPass({ timeout: POLL });
  });

  test("segmented control filter exists", async ({ page }) => {
    await expect(async () => {
      expect(await page.evaluate(shadowExists("sc-channels-view", "sc-segmented-control"))).toBe(
        true,
      );
    }).toPass({ timeout: POLL });
  });

  test("shows channel stats", async ({ page }) => {
    await expect(async () => {
      const count = await page.evaluate(shadowCount("sc-channels-view", "sc-stat-card"));
      expect(count).toBeGreaterThanOrEqual(2);
    }).toPass({ timeout: POLL });
  });

  test("channel names appear in table", async ({ page }) => {
    await expect(async () => {
      const text: string = await page.evaluate(deepText("sc-channels-view"));
      expect(text).toContain("Telegram");
      expect(text).toContain("Discord");
    }).toPass({ timeout: POLL });
  });
});

// ─────────────────────────────────────────────────────────────
// Nodes (Interactions)
// ─────────────────────────────────────────────────────────────
test.describe("Nodes (Interactions)", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/?demo#nodes");
    await page.waitForTimeout(WAIT);
  });

  test("shows node data table", async ({ page }) => {
    await expect(async () => {
      expect(await page.evaluate(shadowExists("sc-nodes-view", "sc-data-table-v2"))).toBe(true);
    }).toPass({ timeout: POLL });
  });

  test("refresh button exists", async ({ page }) => {
    await expect(async () => {
      const hasRefresh = await page.evaluate(`(() => {
        const app = document.querySelector("sc-app");
        const view = app?.shadowRoot?.querySelector("sc-nodes-view");
        const btns = view?.shadowRoot?.querySelectorAll("sc-button") ?? [];
        return [...btns].some(b => {
          const label = b.getAttribute("aria-label") ?? "";
          return label.includes("Refresh");
        });
      })()`);
      expect(hasRefresh).toBe(true);
    }).toPass({ timeout: POLL });
  });

  test("shows node names", async ({ page }) => {
    await expect(async () => {
      const text: string = await page.evaluate(deepText("sc-nodes-view"));
      expect(text).toContain("local");
    }).toPass({ timeout: POLL });
  });
});

// ─────────────────────────────────────────────────────────────
// Logs (Interactions)
// ─────────────────────────────────────────────────────────────
test.describe("Logs (Interactions)", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/?demo#logs");
    await page.waitForTimeout(WAIT);
  });

  test("shows log area", async ({ page }) => {
    await expect(async () => {
      const hasLogArea = await page.evaluate(`(() => {
        const app = document.querySelector("sc-app");
        const view = app?.shadowRoot?.querySelector("sc-logs-view");
        return !!view?.shadowRoot?.querySelector(".log-area") || !!view?.shadowRoot?.querySelector("sc-timeline");
      })()`);
      expect(hasLogArea).toBe(true);
    }).toPass({ timeout: POLL });
  });

  test("level filter exists", async ({ page }) => {
    await expect(async () => {
      expect(await page.evaluate(shadowExists("sc-logs-view", "sc-segmented-control"))).toBe(true);
    }).toPass({ timeout: POLL });
  });

  test("pause/resume button exists", async ({ page }) => {
    await expect(async () => {
      const hasPause = await page.evaluate(`(() => {
        const app = document.querySelector("sc-app");
        const view = app?.shadowRoot?.querySelector("sc-logs-view");
        const btns = view?.shadowRoot?.querySelectorAll("sc-button") ?? [];
        return [...btns].some(b => {
          const label = b.getAttribute("aria-label") ?? "";
          return label === "Pause" || label === "Resume";
        });
      })()`);
      expect(hasPause).toBe(true);
    }).toPass({ timeout: POLL });
  });

  test("clear button exists", async ({ page }) => {
    await expect(async () => {
      const hasClear = await page.evaluate(`(() => {
        const app = document.querySelector("sc-app");
        const view = app?.shadowRoot?.querySelector("sc-logs-view");
        const btns = view?.shadowRoot?.querySelectorAll("sc-button") ?? [];
        return [...btns].some(b => {
          const label = b.getAttribute("aria-label") ?? "";
          return label.includes("Clear");
        });
      })()`);
      expect(hasClear).toBe(true);
    }).toPass({ timeout: POLL });
  });
});

// ─────────────────────────────────────────────────────────────
// Tools (Interactions)
// ─────────────────────────────────────────────────────────────
test.describe("Tools (Interactions)", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/?demo#tools");
    await page.waitForTimeout(WAIT);
  });

  test("shows tools data table", async ({ page }) => {
    await expect(async () => {
      expect(await page.evaluate(shadowExists("sc-tools-view", "sc-data-table-v2"))).toBe(true);
    }).toPass({ timeout: POLL });
  });

  test("shows tool names in table", async ({ page }) => {
    await expect(async () => {
      const text: string = await page.evaluate(deepText("sc-tools-view"));
      expect(text).toContain("shell");
      expect(text).toContain("file_read");
    }).toPass({ timeout: POLL });
  });

  test("shows tool stats", async ({ page }) => {
    await expect(async () => {
      const count = await page.evaluate(shadowCount("sc-tools-view", "sc-stat-card"));
      expect(count).toBeGreaterThanOrEqual(2);
    }).toPass({ timeout: POLL });
  });
});

// ─────────────────────────────────────────────────────────────
// Usage (Interactions)
// ─────────────────────────────────────────────────────────────
test.describe("Usage (Interactions)", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/?demo#usage");
    await page.waitForTimeout(WAIT);
  });

  test("shows usage stats", async ({ page }) => {
    await expect(async () => {
      const count = await page.evaluate(shadowCount("sc-usage-view", "sc-stat-card"));
      expect(count).toBeGreaterThanOrEqual(2);
    }).toPass({ timeout: POLL });
  });

  test("time range segmented control exists", async ({ page }) => {
    await expect(async () => {
      expect(await page.evaluate(shadowExists("sc-usage-view", "sc-segmented-control"))).toBe(true);
    }).toPass({ timeout: POLL });
  });

  test("export button exists", async ({ page }) => {
    await expect(async () => {
      const hasExport = await page.evaluate(`(() => {
        const app = document.querySelector("sc-app");
        const view = app?.shadowRoot?.querySelector("sc-usage-view");
        const btns = view?.shadowRoot?.querySelectorAll("sc-button") ?? [];
        return [...btns].some(b => {
          const label = b.getAttribute("aria-label") ?? "";
          return label.includes("Export");
        });
      })()`);
      expect(hasExport).toBe(true);
    }).toPass({ timeout: POLL });
  });

  test("shows provider breakdown", async ({ page }) => {
    await expect(async () => {
      const text: string = await page.evaluate(deepText("sc-usage-view"));
      expect(text).toContain("openrouter");
      expect(text).toContain("anthropic");
    }).toPass({ timeout: POLL });
  });

  test("shows charts", async ({ page }) => {
    await expect(async () => {
      const count = await page.evaluate(shadowCount("sc-usage-view", "sc-chart"));
      expect(count).toBeGreaterThanOrEqual(1);
    }).toPass({ timeout: POLL });
  });
});
