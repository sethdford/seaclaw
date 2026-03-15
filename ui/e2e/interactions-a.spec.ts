import { test, expect } from "@playwright/test";
import { shadowCount, shadowExists, shadowText, deepText, WAIT, POLL } from "./helpers.js";

test.describe("Chat (Interactions)", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/?demo#chat");
    await page.waitForTimeout(WAIT);
  });

  test("send message and receive demo response", async ({ page }) => {
    await page.evaluate(() => {
      const app = document.querySelector("hu-app");
      const view = app?.shadowRoot?.querySelector("hu-chat-view");
      const composer = view?.shadowRoot?.querySelector("hu-chat-composer");
      if (!composer) return;
      composer.dispatchEvent(
        new CustomEvent("hu-send", {
          bubbles: true,
          composed: true,
          detail: { message: "Hello world" },
        }),
      );
    });

    await expect(async () => {
      const text: string = await page.evaluate(deepText("hu-chat-view"));
      expect(text).toContain("Hello world");
    }).toPass({ timeout: 15000 });
  });

  test("sessions panel toggle works", async ({ page }) => {
    await page.evaluate(() => {
      const app = document.querySelector("hu-app");
      const view = app?.shadowRoot?.querySelector("hu-chat-view");
      const toggle = view?.shadowRoot?.querySelector(".sessions-toggle") as HTMLElement | null;
      toggle?.click();
    });
    await page.waitForTimeout(600);

    await expect(async () => {
      const panelOpen = await page.evaluate(() => {
        const app = document.querySelector("hu-app");
        const view = app?.shadowRoot?.querySelector("hu-chat-view");
        const panel = view?.shadowRoot?.querySelector("hu-chat-sessions-panel");
        return panel?.hasAttribute("open") || (panel as any)?.open === true;
      });
      expect(panelOpen).toBe(true);
    }).toPass({ timeout: POLL });
  });

  test("connected status shows", async ({ page }) => {
    await expect(async () => {
      const text = await page.evaluate(shadowText("hu-chat-view"));
      expect(text).toContain("Connected");
    }).toPass({ timeout: POLL });
  });

  test("chat composer exists with textarea", async ({ page }) => {
    await expect(async () => {
      const has = await page.evaluate(() => {
        const app = document.querySelector("hu-app");
        const view = app?.shadowRoot?.querySelector("hu-chat-view");
        const composer = view?.shadowRoot?.querySelector("hu-chat-composer");
        return !!composer?.shadowRoot?.querySelector("textarea");
      });
      expect(has).toBe(true);
    }).toPass({ timeout: POLL });
  });
});

test.describe("Config (Interactions)", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/?demo#config");
    await page.waitForTimeout(WAIT);
  });

  test("shows save button", async ({ page }) => {
    await expect(async () => {
      const text = await page.evaluate(shadowText("hu-config-view"));
      expect(text).toContain("Save");
    }).toPass({ timeout: POLL });
  });

  test("raw JSON toggle shows code block", async ({ page }) => {
    await expect(async () => {
      const text = await page.evaluate(shadowText("hu-config-view"));
      expect(text).toContain("Raw JSON");
    }).toPass({ timeout: POLL });

    await page.evaluate(() => {
      const app = document.querySelector("hu-app");
      const view = app?.shadowRoot?.querySelector("hu-config-view");
      const btns = view?.shadowRoot?.querySelectorAll("hu-button") ?? [];
      const rawBtn = [...btns].find((b) => b.textContent?.trim().includes("Raw JSON"));
      (rawBtn as HTMLElement)?.click();
    });
    await page.waitForTimeout(600);

    await expect(async () => {
      const hasCodeBlock = await page.evaluate(shadowExists("hu-config-view", "hu-code-block"));
      expect(hasCodeBlock).toBe(true);
    }).toPass({ timeout: POLL });

    await page.evaluate(() => {
      const app = document.querySelector("hu-app");
      const view = app?.shadowRoot?.querySelector("hu-config-view");
      const btns = view?.shadowRoot?.querySelectorAll("hu-button") ?? [];
      const formBtn = [...btns].find((b) => b.textContent?.trim() === "Form");
      (formBtn as HTMLElement)?.click();
    });
    await page.waitForTimeout(400);

    await expect(async () => {
      const text = await page.evaluate(shadowText("hu-config-view"));
      expect(text).toContain("Save");
      expect(text).toContain("Raw JSON");
    }).toPass({ timeout: POLL });
  });

  test("page hero and section headers render", async ({ page }) => {
    await expect(async () => {
      expect(await page.evaluate(shadowExists("hu-config-view", "hu-page-hero"))).toBe(true);
      const count = await page.evaluate(shadowCount("hu-config-view", "hu-section-header"));
      expect(count).toBeGreaterThanOrEqual(1);
    }).toPass({ timeout: POLL });
  });
});

test.describe("Security (Interactions)", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/?demo#security");
    await page.waitForTimeout(WAIT);
  });

  test("shows autonomy level section", async ({ page }) => {
    await expect(async () => {
      const text = await page.evaluate(shadowText("hu-security-view"));
      expect(text).toContain("Autonomy");
    }).toPass({ timeout: POLL });
  });

  test("shows sandbox section", async ({ page }) => {
    await expect(async () => {
      const text = await page.evaluate(shadowText("hu-security-view"));
      expect(text).toContain("Sandbox");
    }).toPass({ timeout: POLL });
  });

  test("shows network proxy section", async ({ page }) => {
    await expect(async () => {
      const text = await page.evaluate(shadowText("hu-security-view"));
      expect(text).toMatch(/Network|Proxy/);
    }).toPass({ timeout: POLL });
  });

  test("has select for autonomy level", async ({ page }) => {
    await expect(async () => {
      expect(await page.evaluate(shadowExists("hu-security-view", "hu-select"))).toBe(true);
    }).toPass({ timeout: POLL });
  });
});

test.describe("Tools (Interactions)", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/?demo#tools");
    await page.waitForTimeout(WAIT);
  });

  test("shows tool list with items", async ({ page }) => {
    await expect(async () => {
      const text: string = await page.evaluate(deepText("hu-tools-view"));
      expect(text).toContain("shell");
    }).toPass({ timeout: POLL });
  });

  test("shows tool descriptions", async ({ page }) => {
    await expect(async () => {
      const text: string = await page.evaluate(deepText("hu-tools-view"));
      expect(text).toContain("Execute shell commands");
    }).toPass({ timeout: POLL });
  });

  test("page hero renders", async ({ page }) => {
    await expect(async () => {
      expect(await page.evaluate(shadowExists("hu-tools-view", "hu-page-hero"))).toBe(true);
    }).toPass({ timeout: POLL });
  });
});

test.describe("Models (Interactions)", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/?demo#models");
    await page.waitForTimeout(WAIT);
  });

  test("shows provider list", async ({ page }) => {
    await expect(async () => {
      const text: string = await page.evaluate(deepText("hu-models-view"));
      expect(text).toContain("openrouter");
      expect(text).toContain("anthropic");
    }).toPass({ timeout: POLL });
  });

  test("shows API key status", async ({ page }) => {
    await expect(async () => {
      const text: string = await page.evaluate(deepText("hu-models-view"));
      expect(text).toMatch(/key|configured|API/i);
    }).toPass({ timeout: POLL });
  });

  test("page hero renders", async ({ page }) => {
    await expect(async () => {
      expect(await page.evaluate(shadowExists("hu-models-view", "hu-page-hero"))).toBe(true);
    }).toPass({ timeout: POLL });
  });
});

test.describe("Sessions (Interactions)", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/?demo#sessions");
    await page.waitForTimeout(WAIT);
  });

  test("shows session list", async ({ page }) => {
    await expect(async () => {
      const text: string = await page.evaluate(deepText("hu-sessions-view"));
      expect(text).toContain("Project Planning");
    }).toPass({ timeout: POLL });
  });

  test("shows session message counts", async ({ page }) => {
    await expect(async () => {
      const text: string = await page.evaluate(deepText("hu-sessions-view"));
      expect(text).toMatch(/24|12|8/);
    }).toPass({ timeout: POLL });
  });

  test("shows last message preview", async ({ page }) => {
    await expect(async () => {
      const text: string = await page.evaluate(deepText("hu-sessions-view"));
      expect(text).toContain("sprint goals");
    }).toPass({ timeout: POLL });
  });

  test("page hero renders", async ({ page }) => {
    await expect(async () => {
      expect(await page.evaluate(shadowExists("hu-sessions-view", "hu-page-hero"))).toBe(true);
    }).toPass({ timeout: POLL });
  });
});
