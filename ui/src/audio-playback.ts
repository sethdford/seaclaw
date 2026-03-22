/**
 * Gapless f32le PCM playback at a fixed sample rate (24 kHz for Cartesia / gateway voice).
 * Uses an inline AudioWorklet loaded from a Blob URL.
 */

const PCM_PULL_WORKLET_SRC = `
class PcmPullProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this._chunks = [];
    this._current = null;
    this._read = 0;
    this._endMarked = false;
    this._endedPosted = false;
    this.port.onmessage = (e) => {
      const t = e.data && e.data.type;
      if (t === "push") {
        this._endedPosted = false;
        this._chunks.push(new Float32Array(e.data.buffer));
      } else if (t === "clear") {
        this._chunks = [];
        this._current = null;
        this._read = 0;
        this._endMarked = false;
        this._endedPosted = false;
      } else if (t === "end") {
        this._endMarked = true;
      }
    };
  }

  _maybePostEnded() {
    const curDone = !this._current || this._read >= this._current.length;
    if (this._endMarked && this._chunks.length === 0 && curDone && !this._endedPosted) {
      this._endedPosted = true;
      this.port.postMessage({ type: "ended" });
      this._endMarked = false;
    }
  }

  process(inputs, outputs) {
    const out = outputs[0][0];
    let i = 0;
    while (i < out.length) {
      if (!this._current || this._read >= this._current.length) {
        this._current = this._chunks.length ? this._chunks.shift() : null;
        this._read = 0;
        if (!this._current) break;
      }
      const take = Math.min(out.length - i, this._current.length - this._read);
      out.set(this._current.subarray(this._read, this._read + take), i);
      this._read += take;
      i += take;
    }
    if (i < out.length) {
      out.fill(0, i);
    }
    this._maybePostEnded();
    return true;
  }
}

registerProcessor("pcm-pull", PcmPullProcessor);
`;

export class AudioPlaybackEngine {
  readonly sampleRate: number;
  #ctx: AudioContext | null = null;
  #node: AudioWorkletNode | null = null;
  #onPlaybackEnd: (() => void) | null = null;

  constructor(sampleRate = 24000) {
    this.sampleRate = sampleRate;
  }

  setOnPlaybackEnd(cb: (() => void) | null): void {
    this.#onPlaybackEnd = cb;
  }

  async #ensureAudio(): Promise<void> {
    if (this.#ctx && this.#node) return;
    const ctx = new AudioContext({ sampleRate: this.sampleRate });
    const blob = new Blob([PCM_PULL_WORKLET_SRC], { type: "application/javascript" });
    const url = URL.createObjectURL(blob);
    await ctx.audioWorklet.addModule(url);
    URL.revokeObjectURL(url);
    const node = new AudioWorkletNode(ctx, "pcm-pull", {
      numberOfOutputs: 1,
      outputChannelCount: [1],
    });
    node.port.onmessage = (e: MessageEvent<{ type?: string }>) => {
      if (e.data?.type === "ended") {
        this.#onPlaybackEnd?.();
      }
    };
    node.connect(ctx.destination);
    this.#ctx = ctx;
    this.#node = node;
  }

  /** Queue one PCM chunk (mono f32). Copies internally before posting to the worklet. */
  async pushChunk(pcm: Float32Array): Promise<void> {
    await this.#ensureAudio();
    if (!this.#node) return;
    const copy = pcm.slice();
    this.#node.port.postMessage({ type: "push", buffer: copy.buffer }, [copy.buffer]);
    await this.#ctx?.resume();
  }

  /** Signal that no further chunks will arrive; fires onPlaybackEnd once the buffer drains. */
  markEndOfStream(): void {
    this.#node?.port.postMessage({ type: "end" });
  }

  /** Drop queued audio and suspend the context. */
  interrupt(): void {
    this.#node?.port.postMessage({ type: "clear" });
    void this.#ctx?.suspend();
  }

  get isPlaying(): boolean {
    return this.#ctx?.state === "running";
  }
}
