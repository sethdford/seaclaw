import { test, expect } from "@playwright/test";

const LIVE = process.env.SEACLAW_LIVE_E2E === "1";

test.describe("Live Gateway E2E", () => {
  test.skip(!LIVE, "Set SEACLAW_LIVE_E2E=1 to run live gateway tests");

  test.beforeEach(async ({ page }) => {
    await page.goto("/#chat");
    const chatView = page.locator("sc-app").locator("sc-chat-view");
    await expect(chatView).toBeAttached({ timeout: 10000 });
  });

  test("gateway connection established", async ({ page }) => {
    const chatView = page.locator("sc-app").locator("sc-chat-view");
    const statusDot = chatView.locator(".status-dot.connected");
    await expect(statusDot).toBeAttached({ timeout: 15000 });

    const statusText = chatView.getByText("Connected");
    await expect(statusText).toBeVisible({ timeout: 5000 });

    const textarea = chatView.locator("textarea");
    await expect(textarea).toBeEnabled({ timeout: 5000 });
  });

  test("send message and receive AI response", async ({ page }) => {
    const chatView = page.locator("sc-app").locator("sc-chat-view");
    await expect(chatView.locator(".status-dot.connected")).toBeAttached({
      timeout: 15000,
    });

    const textarea = chatView.locator("textarea");
    await textarea.fill("What is 2+2? Reply with just the number.");
    const sendBtn = chatView.locator(".send-btn");
    await sendBtn.click();

    const assistantMsg = chatView.locator(".message.assistant").first();
    await expect(assistantMsg).toBeVisible({ timeout: 30000 });

    const content = await assistantMsg.textContent();
    expect(content).toContain("4");
  });

  test("streaming indicator appears during response", async ({ page }) => {
    const chatView = page.locator("sc-app").locator("sc-chat-view");
    await expect(chatView.locator(".status-dot.connected")).toBeAttached({
      timeout: 15000,
    });

    const textarea = chatView.locator("textarea");
    await textarea.fill("Count from 1 to 10 slowly, one number per line.");
    const sendBtn = chatView.locator(".send-btn");
    await sendBtn.click();

    const thinkingOrStream = chatView.locator("sc-thinking, .message.assistant");
    await expect(thinkingOrStream.first()).toBeVisible({ timeout: 15000 });

    const assistantMsg = chatView.locator(".message.assistant").first();
    await expect(assistantMsg).toBeVisible({ timeout: 30000 });
  });

  test("multi-turn conversation maintains context", async ({ page }) => {
    const chatView = page.locator("sc-app").locator("sc-chat-view");
    await expect(chatView.locator(".status-dot.connected")).toBeAttached({
      timeout: 15000,
    });

    const textarea = chatView.locator("textarea");
    const sendBtn = chatView.locator(".send-btn");

    await textarea.fill("Remember this number: 42. Reply OK.");
    await sendBtn.click();
    const firstReply = chatView.locator(".message.assistant").first();
    await expect(firstReply).toBeVisible({ timeout: 30000 });

    await textarea.fill("What number did I ask you to remember?");
    await sendBtn.click();

    await page.waitForTimeout(2000);
    const replies = chatView.locator(".message.assistant");
    const lastReply = replies.last();
    await expect(lastReply).toBeVisible({ timeout: 30000 });

    const content = await lastReply.textContent();
    expect(content).toContain("42");
  });

  test("connected chat screenshot", async ({ page }) => {
    const chatView = page.locator("sc-app").locator("sc-chat-view");
    await expect(chatView.locator(".status-dot.connected")).toBeAttached({
      timeout: 15000,
    });

    const textarea = chatView.locator("textarea");
    await textarea.fill("Say hello in one sentence.");
    const sendBtn = chatView.locator(".send-btn");
    await sendBtn.click();

    const assistantMsg = chatView.locator(".message.assistant").first();
    await expect(assistantMsg).toBeVisible({ timeout: 30000 });

    await page.screenshot({
      path: "e2e-results/live-gateway-chat.png",
      fullPage: true,
    });
  });
});
