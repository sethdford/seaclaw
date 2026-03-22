import { describe, it, expect, vi, beforeEach, afterEach } from "vitest";

type PortMessage = { type: string; buffer?: ArrayBuffer };

class MockWorkletPort {
  onmessage: ((e: { data: PortMessage }) => void) | null = null;
  messages: PortMessage[] = [];
  postMessage(msg: PortMessage) {
    this.messages.push(msg);
  }
  simulateMessage(data: PortMessage) {
    this.onmessage?.({ data });
  }
}

class MockAudioWorkletNode {
  port = new MockWorkletPort();
  connected = true;
  disconnect() {
    this.connected = false;
  }
}

class MockAudioContext {
  sampleRate: number;
  state = "running" as AudioContextState;
  audioWorklet = {
    addModule: vi.fn().mockResolvedValue(undefined),
  };
  destination = {} as AudioDestinationNode;
  private _node: MockAudioWorkletNode | null = null;

  constructor(opts?: { sampleRate?: number }) {
    this.sampleRate = opts?.sampleRate ?? 24000;
  }
  resume = vi.fn().mockResolvedValue(undefined);
  suspend = vi.fn().mockImplementation(() => {
    this.state = "suspended";
    return Promise.resolve();
  });
  close = vi.fn().mockImplementation(() => {
    this.state = "closed";
    return Promise.resolve();
  });

  _getNode(): MockAudioWorkletNode | null {
    return this._node;
  }
  _setNode(n: MockAudioWorkletNode) {
    this._node = n;
  }
}

let createdContexts: MockAudioContext[] = [];
let createdNodes: MockAudioWorkletNode[] = [];

function setupGlobals() {
  createdContexts = [];
  createdNodes = [];

  globalThis.AudioContext = class extends MockAudioContext {
    constructor(opts?: { sampleRate?: number }) {
      super(opts);
      createdContexts.push(this);
    }
  } as unknown as typeof AudioContext;

  globalThis.AudioWorkletNode = class extends MockAudioWorkletNode {
    constructor(_ctx: MockAudioContext, _name: string, _opts?: unknown) {
      super();
      createdNodes.push(this);
      _ctx._setNode(this);
      this.connect = vi.fn();
    }
    connect: ReturnType<typeof vi.fn>;
  } as unknown as typeof AudioWorkletNode;

  globalThis.Blob = class {
    constructor(
      public parts: unknown[],
      public opts?: { type?: string },
    ) {}
  } as unknown as typeof Blob;

  globalThis.URL.createObjectURL = vi.fn(() => "blob:mock");
  globalThis.URL.revokeObjectURL = vi.fn();
}

function teardownGlobals() {
  delete (globalThis as Record<string, unknown>).AudioContext;
  delete (globalThis as Record<string, unknown>).AudioWorkletNode;
}

describe("AudioPlaybackEngine", () => {
  beforeEach(() => {
    vi.useFakeTimers();
    setupGlobals();
  });

  afterEach(() => {
    teardownGlobals();
    vi.useRealTimers();
  });

  async function createEngine() {
    const { AudioPlaybackEngine } = await import("./audio-playback.js");
    return new AudioPlaybackEngine(24000);
  }

  it("creates AudioContext on first pushChunk", async () => {
    const engine = await createEngine();
    expect(createdContexts).toHaveLength(0);
    await engine.pushChunk(new Float32Array([0.1, 0.2]));
    expect(createdContexts).toHaveLength(1);
    expect(createdContexts[0].sampleRate).toBe(24000);
    engine.dispose();
  });

  it("posts push messages to the worklet port", async () => {
    const engine = await createEngine();
    await engine.pushChunk(new Float32Array([0.5]));
    const port = createdNodes[0].port;
    const pushMsgs = port.messages.filter((m) => m.type === "push");
    expect(pushMsgs).toHaveLength(1);
    engine.dispose();
  });

  it("resumes context on pushChunk", async () => {
    const engine = await createEngine();
    await engine.pushChunk(new Float32Array([0.1]));
    expect(createdContexts[0].resume).toHaveBeenCalled();
    engine.dispose();
  });

  it("reuses existing AudioContext on subsequent pushChunk calls", async () => {
    const engine = await createEngine();
    await engine.pushChunk(new Float32Array([0.1]));
    await engine.pushChunk(new Float32Array([0.2]));
    expect(createdContexts).toHaveLength(1);
    engine.dispose();
  });

  it("markEndOfStream posts end message", async () => {
    const engine = await createEngine();
    await engine.pushChunk(new Float32Array([0.1]));
    engine.markEndOfStream();
    const port = createdNodes[0].port;
    expect(port.messages.some((m) => m.type === "end")).toBe(true);
    engine.dispose();
  });

  it("interrupt posts clear and suspends context", async () => {
    const engine = await createEngine();
    await engine.pushChunk(new Float32Array([0.1]));
    engine.interrupt();
    const port = createdNodes[0].port;
    expect(port.messages.some((m) => m.type === "clear")).toBe(true);
    expect(createdContexts[0].suspend).toHaveBeenCalled();
    engine.dispose();
  });

  it("fires onPlaybackEnd when worklet posts ended", async () => {
    const engine = await createEngine();
    const cb = vi.fn();
    engine.setOnPlaybackEnd(cb);
    await engine.pushChunk(new Float32Array([0.1]));
    createdNodes[0].port.simulateMessage({ type: "ended" });
    expect(cb).toHaveBeenCalledOnce();
    engine.dispose();
  });

  it("dispose closes context and disconnects node", async () => {
    const engine = await createEngine();
    await engine.pushChunk(new Float32Array([0.1]));
    const ctx = createdContexts[0];
    const node = createdNodes[0];
    engine.dispose();
    expect(ctx.close).toHaveBeenCalled();
    expect(node.connected).toBe(false);
  });

  it("idle timer tears down AudioContext after timeout", async () => {
    const engine = await createEngine();
    await engine.pushChunk(new Float32Array([0.1]));
    engine.markEndOfStream();
    const ctx = createdContexts[0];
    expect(ctx.close).not.toHaveBeenCalled();
    vi.advanceTimersByTime(60_000);
    expect(ctx.close).toHaveBeenCalled();
  });

  it("pushChunk resets idle timer", async () => {
    const engine = await createEngine();
    await engine.pushChunk(new Float32Array([0.1]));
    engine.markEndOfStream();
    vi.advanceTimersByTime(30_000);
    const ctx = createdContexts[0];
    expect(ctx.close).not.toHaveBeenCalled();
    await engine.pushChunk(new Float32Array([0.2]));
    vi.advanceTimersByTime(59_999);
    expect(ctx.close).not.toHaveBeenCalled();
    vi.advanceTimersByTime(2);
    expect(ctx.close).not.toHaveBeenCalled();
    engine.markEndOfStream();
    vi.advanceTimersByTime(60_000);
    expect(ctx.close).toHaveBeenCalled();
  });

  it("isPlaying reflects context state", async () => {
    const engine = await createEngine();
    expect(engine.isPlaying).toBe(false);
    await engine.pushChunk(new Float32Array([0.1]));
    expect(engine.isPlaying).toBe(true);
    engine.interrupt();
    expect(engine.isPlaying).toBe(false);
    engine.dispose();
  });

  it("markEndOfStream is safe before any audio is pushed", async () => {
    const engine = await createEngine();
    expect(() => engine.markEndOfStream()).not.toThrow();
    engine.dispose();
  });

  it("interrupt is safe before any audio is pushed", async () => {
    const engine = await createEngine();
    expect(() => engine.interrupt()).not.toThrow();
    engine.dispose();
  });
});
