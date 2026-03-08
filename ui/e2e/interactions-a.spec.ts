import { test, expect } from "@playwright/test";
import { shadowCount, shadowExists, shadowText, waitForViewReady, POLL } from "./helpers.js";

// ─────────────────────────────────────────────────────────────
// Chat View (Interactions)
// ─────────────────────────────────────────────────────────────
test.describe("Chat (Interactions)", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/?demo#chat");
    await waitForViewReady(page, "sc-chat-view");
  });

  test("send message and receive demo response", async ({ page }) => {
    await page.evaluate(() => {
      const app = document.querySelector("sc-app");
      const view = app?.shadowRoot?.querySelector("sc-chat-view");
      const composer = view?.shadowRoot?.querySelector("sc-chat-composer") as Element | null;
      const textarea = composer?.shadowRoot?.querySelector(
        "textarea",
      ) as HTMLTextAreaElement | null;
      if (!textarea) return;
      textarea.focus();
      textarea.value = "Hello world";
      textarea.dispatchEvent(new Event("input", { bubbles: true }));
    });

    await page.evaluate(() => {
      const app = document.querySelector("sc-app");
      const view = app?.shadowRoot?.querySelector("sc-chat-view");
      const composer = view?.shadowRoot?.querySelector("sc-chat-composer") as Element | null;
      const btn = composer?.shadowRoot?.querySelector(".send-btn.send") as HTMLElement | null;
      btn?.click();
    });

    await expect(async () => {
      const text: string = await page.evaluate(shadowText("sc-chat-view"));
      expect(text).toContain("Demo response to: Hello world");
    }).toPass({ timeout: 12000 });
  });

  test("sessions panel toggle works", async ({ page }) => {
    await page.evaluate(() => {
      const app = document.querySelector("sc-app");
      const view = app?.shadowRoot?.querySelector("sc-chat-view");
      const toggle = view?.shadowRoot?.querySelector(".sessions-toggle") as HTMLElement | null;
      toggle?.click();
    });

    await expect(async () => {
      const panelOpen = await page.evaluate(() => {
        const app = document.querySelector("sc-app");
        const view = app?.shadowRoot?.querySelector("sc-chat-view");
        const panel = view?.shadowRoot?.querySelector("sc-chat-sessions-panel");
        return panel?.hasAttribute("open") || (panel as any)?.open === true;
      });
      expect(panelOpen).toBe(true);
    }).toPass({ timeout: POLL });
  });

  test("connected status shows", async ({ page }) => {
    await expect(async () => {
      const text = await page.evaluate(shadowText("sc-chat-view"));
      expect(text).toContain("Connected");
    }).toPass({ timeout: POLL });
  });

  test("chat composer exists with textarea", async ({ page }) => {
    await expect(async () => {
      const has = await page.evaluate(() => {
        const app = document.querySelector("sc-app");
        const view = app?.shadowRoot?.querySelector("sc-chat-view");
        const composer = view?.shadowRoot?.querySelector("sc-chat-composer");
        return !!composer?.shadowRoot?.querySelector("textarea");
      });
      expect(has).toBe(true);
    }).toPass({ timeout: POLL });
  });
});

// ─────────────────────────────────────────────────────────────
// Config View (Interactions)
// ─────────────────────────────────────────────────────────────
test.describe("Config (Interactions)", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/?demo#config");
    await waitForViewReady(page, "sc-config-view");
  });

  test("shows save button", async ({ page }) => {
    await expect(async () => {
      const text = await page.evaluate(shadowText("sc-config-view"));
      expect(text).toContain("Save");
    }).toPass({ timeout: POLL });
  });

  test("raw JSON toggle shows code block", async ({ page }) => {
    await expect(async () => {
      const text = await page.evaluate(shadowText("sc-config-view"));
      expect(text).toContain("Raw JSON");
    }).toPass({ timeout: POLL });

    await page.evaluate(() => {
      const app = document.querySelector("sc-app");
      const view = app?.shadowRoot?.querySelector("sc-config-view");
      const btns = view?.shadowRoot?.querySelectorAll("sc-button") ?? [];
      const rawBtn = [...btns].find((b) => b.textContent?.trim().includes("Raw JSON"));
      (rawBtn as HTMLElement)?.click();
    });

    await expect(async () => {
      const hasCodeBlock = await page.evaluate(shadowExists("sc-config-view", "sc-code-block"));
      expect(hasCodeBlock).toBe(true);
    }).toPass({ timeout: POLL });

    await page.evaluate(() => {
      const app = document.querySelector("sc-app");
      const view = app?.shadowRoot?.querySelector("sc-config-view");
      const btns = view?.shadowRoot?.querySelectorAll("sc-button") ?? [];
      const formBtn = [...btns].find((b) => b.textContent?.trim() === "Form");
      (formBtn as HTMLElement)?.click();
    });

    await expect(async () => {
      const text = await page.evaluate(shadowText("sc-config-view"));
      expect(text).toContain("Save");
      expect(text).toContain("Raw JSON");
    }).toPass({ timeout: POLL });
  });

  test("page hero and section headers render", async ({ page }) => {
    await expect(async () => {
      expect(await page.evaluate(shadowExists("sc-config-view", "sc-page-hero"))).toBe(true);
      const count = await page.evaluate(shadowCount("sc-config-view", "sc-section-header"));
      expect(count).toBeGreaterThanOrEqual(1);
    }).toPass({ timeout: POLL });
  });
});

// ─────────────────────────────────────────────────────────────
// Security View (Interactions)
// ─────────────────────────────────────────────────────────────
test.describe("Security (Interactions)", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/?demo#security");
    await waitForViewReady(page, "sc-security-view");
  });

  test("shows autonomy level section", async ({ page }) => {
    await expect(async () => {
      const text = await page.evaluate(shadowText("sc-security-view"));
      expect(text).toContain("Autonomy");
    }).toPass({ timeout: POLL });
  });

  test("shows sandbox section", async ({ page }) => {
    await expect(async () => {
      const text = await page.evaluate(shadowText("sc-security-view"));
      expect(text).toContain("Sandbox");
    }).toPass({ timeout: POLL });
  });

  test("shows network proxy section", async ({ page }) => {
    await expect(async () => {
      const text = await page.evaluate(shadowText("sc-security-view"));
      expect(text).toMatch(/Network|Proxy/);
    }).toPass({ timeout: POLL });
  });

  test("has select for autonomy level", async ({ page }) => {
    await expect(async () => {
      expect(await page.evaluate(shadowExists("sc-security-view", "sc-select"))).toBe(true);
    }).toPass({ timeout: POLL });
  });
});
