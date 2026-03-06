import { describe, it, expect } from "vitest";

import "./floating-mic.js";
import "./sidebar.js";
import "./command-palette.js";
import "./sc-welcome.js";
import "./sc-sparkline.js";
import "./sc-animated-icon.js";
import "./sc-animated-number.js";
import "./sc-activity-feed.js";

describe("sc-floating-mic", () => {
  it("should be defined as a custom element", () => {
    expect(customElements.get("sc-floating-mic")).toBeDefined();
  });

  it("should be creatable", () => {
    const el = document.createElement("sc-floating-mic");
    expect(el).toBeInstanceOf(HTMLElement);
  });
});

describe("sc-sidebar", () => {
  it("should be defined as a custom element", () => {
    expect(customElements.get("sc-sidebar")).toBeDefined();
  });

  it("should be creatable", () => {
    const el = document.createElement("sc-sidebar");
    expect(el).toBeInstanceOf(HTMLElement);
  });
});

describe("sc-command-palette", () => {
  it("should be defined as a custom element", () => {
    expect(customElements.get("sc-command-palette")).toBeDefined();
  });

  it("should be creatable", () => {
    const el = document.createElement("sc-command-palette");
    expect(el).toBeInstanceOf(HTMLElement);
  });
});

describe("sc-welcome", () => {
  it("should be defined as a custom element", () => {
    expect(customElements.get("sc-welcome")).toBeDefined();
  });

  it("should be creatable", () => {
    const el = document.createElement("sc-welcome");
    expect(el).toBeInstanceOf(HTMLElement);
  });
});

describe("sc-sparkline", () => {
  it("should be defined as a custom element", () => {
    expect(customElements.get("sc-sparkline")).toBeDefined();
  });

  it("should render with default props", () => {
    const el = document.createElement("sc-sparkline") as HTMLElement & {
      data: number[];
      width: number;
      height: number;
    };
    expect(el.data).toEqual([]);
    expect(el.width).toBe(80);
    expect(el.height).toBe(28);
  });

  it("should accept data array", () => {
    const el = document.createElement("sc-sparkline") as HTMLElement & { data: number[] };
    el.data = [1, 5, 3, 8, 2];
    expect(el.data).toEqual([1, 5, 3, 8, 2]);
  });
});

describe("sc-animated-icon", () => {
  it("should be defined as a custom element", () => {
    expect(customElements.get("sc-animated-icon")).toBeDefined();
  });

  it("should have default icon and state", () => {
    const el = document.createElement("sc-animated-icon") as HTMLElement & {
      icon: string;
      state: string;
    };
    expect(el.icon).toBe("check");
    expect(el.state).toBe("idle");
  });
});

describe("sc-animated-number", () => {
  it("should be defined as a custom element", () => {
    expect(customElements.get("sc-animated-number")).toBeDefined();
  });

  it("should have default value 0", () => {
    const el = document.createElement("sc-animated-number") as HTMLElement & {
      value: number;
      suffix: string;
      prefix: string;
    };
    expect(el.value).toBe(0);
    expect(el.suffix).toBe("");
    expect(el.prefix).toBe("");
  });

  it("should accept value property", () => {
    const el = document.createElement("sc-animated-number") as HTMLElement & { value: number };
    el.value = 42;
    expect(el.value).toBe(42);
  });
});

describe("sc-activity-feed", () => {
  it("should be defined as a custom element", () => {
    expect(customElements.get("sc-activity-feed")).toBeDefined();
  });

  it("should have default empty events and max 6", () => {
    const el = document.createElement("sc-activity-feed") as HTMLElement & {
      events: unknown[];
      max: number;
    };
    expect(el.events).toEqual([]);
    expect(el.max).toBe(6);
  });
});
