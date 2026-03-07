import { describe, it, expect, vi, beforeEach } from "vitest";
import "./sc-composer.js";

type ScComposerEl = HTMLElement & {
  value: string;
  waiting: boolean;
  disabled: boolean;
  showSuggestions: boolean;
  updateComplete: Promise<boolean>;
};

describe("sc-composer", () => {
  beforeEach(async () => {
    await import("./sc-composer.js");
  });

  it("registers as custom element", () => {
    expect(customElements.get("sc-composer")).toBeDefined();
  });

  it("renders textarea and send button", async () => {
    const el = document.createElement("sc-composer") as ScComposerEl;
    document.body.appendChild(el);
    await el.updateComplete;
    const textarea = el.shadowRoot?.querySelector("textarea");
    const sendBtn = el.shadowRoot?.querySelector(".send-btn");
    expect(textarea).toBeTruthy();
    expect(sendBtn).toBeTruthy();
    expect(sendBtn?.textContent?.trim()).toBe("Send");
    el.remove();
  });

  it("disables send when empty", async () => {
    const el = document.createElement("sc-composer") as ScComposerEl;
    document.body.appendChild(el);
    await el.updateComplete;
    const sendBtn = el.shadowRoot?.querySelector(".send-btn") as HTMLButtonElement;
    expect(sendBtn.disabled).toBe(true);
    el.remove();
  });

  it("disables send when waiting", async () => {
    const el = document.createElement("sc-composer") as ScComposerEl;
    el.value = "hello";
    el.waiting = true;
    document.body.appendChild(el);
    await el.updateComplete;
    const sendBtn = el.shadowRoot?.querySelector(".send-btn") as HTMLButtonElement;
    expect(sendBtn.disabled).toBe(true);
    el.remove();
  });

  it("disables input when disabled", async () => {
    const el = document.createElement("sc-composer") as ScComposerEl;
    el.disabled = true;
    document.body.appendChild(el);
    await el.updateComplete;
    const textarea = el.shadowRoot?.querySelector("textarea") as HTMLTextAreaElement;
    expect(textarea.disabled).toBe(true);
    el.remove();
  });

  it("fires sc-send on Enter key", async () => {
    const onSend = vi.fn();
    const el = document.createElement("sc-composer") as ScComposerEl;
    el.value = "test message";
    el.addEventListener("sc-send", onSend);
    document.body.appendChild(el);
    await el.updateComplete;
    const textarea = el.shadowRoot?.querySelector("textarea") as HTMLTextAreaElement;
    textarea.dispatchEvent(
      new KeyboardEvent("keydown", { key: "Enter", shiftKey: false, bubbles: true }),
    );
    expect(onSend).toHaveBeenCalledTimes(1);
    expect(onSend.mock.calls[0][0].detail.message).toBe("test message");
    el.remove();
  });

  it("does not fire sc-send on Shift+Enter", async () => {
    const onSend = vi.fn();
    const el = document.createElement("sc-composer") as ScComposerEl;
    el.value = "test";
    el.addEventListener("sc-send", onSend);
    document.body.appendChild(el);
    await el.updateComplete;
    const textarea = el.shadowRoot?.querySelector("textarea") as HTMLTextAreaElement;
    textarea.dispatchEvent(
      new KeyboardEvent("keydown", { key: "Enter", shiftKey: true, bubbles: true }),
    );
    expect(onSend).not.toHaveBeenCalled();
    el.remove();
  });

  it("fires sc-send on send button click", async () => {
    const onSend = vi.fn();
    const el = document.createElement("sc-composer") as ScComposerEl;
    el.value = "hello";
    el.addEventListener("sc-send", onSend);
    document.body.appendChild(el);
    await el.updateComplete;
    const sendBtn = el.shadowRoot?.querySelector(".send-btn") as HTMLButtonElement;
    sendBtn.click();
    expect(onSend).toHaveBeenCalledTimes(1);
    expect(onSend.mock.calls[0][0].detail.message).toBe("hello");
    el.remove();
  });

  it("shows file attachment button", async () => {
    const el = document.createElement("sc-composer") as ScComposerEl;
    document.body.appendChild(el);
    await el.updateComplete;
    const attachBtn = el.shadowRoot?.querySelector(".attach-btn");
    expect(attachBtn).toBeTruthy();
    expect(attachBtn?.getAttribute("aria-label")).toBe("Attach file");
    el.remove();
  });

  it("renders prompt pills when showSuggestions is true", async () => {
    const el = document.createElement("sc-composer") as ScComposerEl;
    el.showSuggestions = true;
    document.body.appendChild(el);
    await el.updateComplete;
    const pills = el.shadowRoot?.querySelectorAll(".prompt-pill") ?? [];
    expect(pills.length).toBe(4);
    expect(pills[0]?.textContent).toContain("Explain how this project is architected");
    expect(pills[1]?.textContent).toContain("Write a Python web scraper");
    expect(pills[2]?.textContent).toContain("Help me debug an issue");
    expect(pills[3]?.textContent).toContain("What can you do?");
    el.remove();
  });

  it("fires sc-use-suggestion when pill clicked", async () => {
    const onSuggestion = vi.fn();
    const el = document.createElement("sc-composer") as ScComposerEl;
    el.showSuggestions = true;
    el.addEventListener("sc-use-suggestion", onSuggestion);
    document.body.appendChild(el);
    await el.updateComplete;
    const pills = el.shadowRoot?.querySelectorAll(".prompt-pill") ?? [];
    (pills[0] as HTMLButtonElement).click();
    expect(onSuggestion).toHaveBeenCalledTimes(1);
    expect(onSuggestion.mock.calls[0][0].detail.text).toBe(
      "Explain how this project is architected",
    );
    el.remove();
  });

  it("shows character count", async () => {
    const el = document.createElement("sc-composer") as ScComposerEl;
    el.value = "hi";
    document.body.appendChild(el);
    await el.updateComplete;
    const charCount = el.shadowRoot?.querySelector(".char-count");
    expect(charCount).toBeTruthy();
    expect(charCount?.textContent).toContain("2 characters");
    el.remove();
  });
});
