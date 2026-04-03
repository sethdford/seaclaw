/**
 * Shared audio recording utility using MediaRecorder API.
 * Works across Chrome, Firefox, Safari (14.5+), and Edge.
 * Replaces the browser-specific Web Speech API for voice capture.
 */

export interface AudioCaptureResult {
  blob: Blob;
  mimeType: string;
}

export interface StreamingOptions {
  onLevel?: (rms: number) => void;
}

const PREFERRED_MIME_TYPES = [
  "audio/webm;codecs=opus",
  "audio/webm",
  "audio/mp4",
  "audio/ogg;codecs=opus",
];

function selectMimeType(): string {
  if (typeof MediaRecorder === "undefined") return "";
  for (const mt of PREFERRED_MIME_TYPES) {
    if (MediaRecorder.isTypeSupported(mt)) return mt;
  }
  return "";
}

export class AudioRecorder {
  #stream: MediaStream | null = null;
  #recorder: MediaRecorder | null = null;
  #chunks: Blob[] = [];
  #mimeType = "";
  #recording = false;
  #streaming = false;
  #onStreamChunk: ((data: ArrayBuffer) => void) | null = null;
  #levelContext: AudioContext | null = null;
  #levelAnalyser: AnalyserNode | null = null;
  #levelRaf: number | null = null;
  #onLevel: ((rms: number) => void) | null = null;
  /* Raw PCM16 streaming for Gemini Live (16kHz int16 LE) */
  #rawPcmContext: AudioContext | null = null;
  #rawPcmNode: ScriptProcessorNode | null = null;
  #rawPcmMode = false;

  get isRecording(): boolean {
    return this.#recording;
  }

  get isSupported(): boolean {
    const hasMic =
      typeof navigator !== "undefined" && typeof navigator.mediaDevices !== "undefined";
    const hasMediaRecorder = typeof MediaRecorder !== "undefined" && selectMimeType() !== "";
    const hasAudioContext = typeof AudioContext !== "undefined";
    return hasMic && (hasMediaRecorder || hasAudioContext);
  }

  /**
   * Stream encoded media chunks (~250 ms) to the callback while recording.
   * Stops the mic only when {@link stopStreaming} is called.
   */
  async startStreaming(
    onChunk: (data: ArrayBuffer) => void,
    options?: StreamingOptions,
  ): Promise<void> {
    if (this.#recording) return;

    const mimeType = selectMimeType();
    if (!mimeType) throw new Error("No supported audio MIME type found");

    const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
    this.#stream = stream;
    this.#mimeType = mimeType;
    this.#streaming = true;
    this.#onStreamChunk = onChunk;
    this.#onLevel = options?.onLevel ?? null;

    if (this.#onLevel && typeof AudioContext !== "undefined") {
      const ctx = new AudioContext();
      this.#levelContext = ctx;
      const source = ctx.createMediaStreamSource(stream);
      const analyser = ctx.createAnalyser();
      analyser.fftSize = 256;
      source.connect(analyser);
      this.#levelAnalyser = analyser;
      void ctx.resume();
      const samples = new Uint8Array(analyser.fftSize);
      const tick = (): void => {
        if (!this.#levelAnalyser || !this.#onLevel) return;
        this.#levelAnalyser.getByteTimeDomainData(samples);
        let sum = 0;
        for (let i = 0; i < samples.length; i++) {
          const v = (samples[i]! - 128) / 128;
          sum += v * v;
        }
        this.#onLevel(Math.sqrt(sum / samples.length));
        this.#levelRaf = requestAnimationFrame(tick);
      };
      this.#levelRaf = requestAnimationFrame(tick);
    }

    const recorder = new MediaRecorder(stream, { mimeType });
    recorder.ondataavailable = (e: BlobEvent) => {
      if (e.data.size === 0 || !this.#onStreamChunk) return;
      void e.data.arrayBuffer().then((ab) => this.#onStreamChunk?.(ab));
    };
    recorder.start(250);
    this.#recorder = recorder;
    this.#recording = true;
  }

  /**
   * Stream raw PCM16 (16kHz, int16 LE) for Gemini Live native voice.
   * Captures at native rate and downsamples to 16kHz before sending.
   */
  async startRawPcmStreaming(
    onChunk: (data: ArrayBuffer) => void,
    options?: StreamingOptions,
  ): Promise<void> {
    if (this.#recording) return;

    const stream = await navigator.mediaDevices.getUserMedia({
      audio: { sampleRate: 16000, channelCount: 1, echoCancellation: true },
    });
    this.#stream = stream;
    this.#streaming = true;
    this.#recording = true;
    this.#rawPcmMode = true;
    this.#onStreamChunk = onChunk;
    this.#onLevel = options?.onLevel ?? null;

    const ctx = new AudioContext({ sampleRate: 16000 });
    this.#rawPcmContext = ctx;
    const source = ctx.createMediaStreamSource(stream);

    if (this.#onLevel) {
      const analyser = ctx.createAnalyser();
      analyser.fftSize = 256;
      source.connect(analyser);
      this.#levelAnalyser = analyser;
      const samples = new Uint8Array(analyser.fftSize);
      const tickLevel = (): void => {
        if (!this.#levelAnalyser || !this.#onLevel) return;
        this.#levelAnalyser.getByteTimeDomainData(samples);
        let sum = 0;
        for (let i = 0; i < samples.length; i++) {
          const v = (samples[i]! - 128) / 128;
          sum += v * v;
        }
        this.#onLevel(Math.sqrt(sum / samples.length));
        this.#levelRaf = requestAnimationFrame(tickLevel);
      };
      this.#levelRaf = requestAnimationFrame(tickLevel);
    }

    /* ScriptProcessorNode: 4096 samples @ 16kHz ≈ 256ms chunks.
       A GainNode at 0 prevents mic audio from reaching speakers while
       keeping the processor alive (it requires a connected destination). */
    const processor = ctx.createScriptProcessor(4096, 1, 1);
    processor.onaudioprocess = (e: AudioProcessingEvent) => {
      if (!this.#onStreamChunk) return;
      const float32 = e.inputBuffer.getChannelData(0);
      const int16 = new Int16Array(float32.length);
      for (let i = 0; i < float32.length; i++) {
        const s = Math.max(-1, Math.min(1, float32[i]!));
        int16[i] = s < 0 ? s * 0x8000 : s * 0x7fff;
      }
      this.#onStreamChunk(int16.buffer);
    };
    source.connect(processor);
    const silencer = ctx.createGain();
    silencer.gain.value = 0;
    processor.connect(silencer);
    silencer.connect(ctx.destination);
    this.#rawPcmNode = processor;
    void ctx.resume();
  }

  async stopStreaming(): Promise<void> {
    if (!this.#recording || !this.#streaming) {
      throw new Error("Not streaming");
    }

    /* Raw PCM mode: clean up AudioContext + ScriptProcessor */
    if (this.#rawPcmMode) {
      this.#stopLevelMonitor();
      if (this.#rawPcmNode) {
        this.#rawPcmNode.disconnect();
        this.#rawPcmNode = null;
      }
      if (this.#rawPcmContext && this.#rawPcmContext.state !== "closed") {
        void this.#rawPcmContext.close();
      }
      this.#rawPcmContext = null;
      this.#rawPcmMode = false;
      this.#streaming = false;
      this.#onStreamChunk = null;
      this.#onLevel = null;
      this.#recording = false;
      this.#releaseStream();
      return;
    }

    if (!this.#recorder) {
      throw new Error("Not streaming");
    }
    return new Promise((resolve, reject) => {
      const recorder = this.#recorder!;
      recorder.onstop = () => {
        this.#stopLevelMonitor();
        this.#streaming = false;
        this.#onStreamChunk = null;
        this.#onLevel = null;
        this.#recording = false;
        this.#releaseStream();
        this.#recorder = null;
        resolve();
      };
      recorder.onerror = () => {
        this.#stopLevelMonitor();
        this.#streaming = false;
        this.#onStreamChunk = null;
        this.#onLevel = null;
        this.#recording = false;
        this.#releaseStream();
        this.#recorder = null;
        reject(new Error("Streaming recording failed"));
      };
      try {
        recorder.stop();
      } catch {
        this.#stopLevelMonitor();
        this.#streaming = false;
        this.#onStreamChunk = null;
        this.#onLevel = null;
        this.#recording = false;
        this.#releaseStream();
        this.#recorder = null;
        reject(new Error("Failed to stop recorder"));
      }
    });
  }

  get streamMimeType(): string {
    return this.#mimeType;
  }

  async start(): Promise<void> {
    if (this.#recording) return;

    const mimeType = selectMimeType();
    if (!mimeType) throw new Error("No supported audio MIME type found");

    const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
    this.#stream = stream;
    this.#mimeType = mimeType;
    this.#chunks = [];

    const recorder = new MediaRecorder(stream, { mimeType });
    recorder.ondataavailable = (e: BlobEvent) => {
      if (e.data.size > 0) this.#chunks.push(e.data);
    };
    recorder.start(250);
    this.#recorder = recorder;
    this.#recording = true;
  }

  async stop(): Promise<AudioCaptureResult> {
    if (!this.#recorder || !this.#recording) {
      throw new Error("Not recording");
    }

    return new Promise<AudioCaptureResult>((resolve, reject) => {
      const recorder = this.#recorder!;

      recorder.onstop = () => {
        const blob = new Blob(this.#chunks, { type: this.#mimeType });
        const mimeType = this.#mimeType.split(";")[0];
        this.#chunks = [];
        this.#recording = false;
        this.#releaseStream();
        resolve({ blob, mimeType });
      };

      recorder.onerror = () => {
        this.#recording = false;
        this.#releaseStream();
        reject(new Error("Recording failed"));
      };

      recorder.stop();
    });
  }

  dispose(): void {
    if (this.#rawPcmNode) {
      this.#rawPcmNode.disconnect();
      this.#rawPcmNode = null;
    }
    if (this.#rawPcmContext && this.#rawPcmContext.state !== "closed") {
      void this.#rawPcmContext.close();
    }
    this.#rawPcmContext = null;
    this.#rawPcmMode = false;
    if (this.#recorder && this.#recording) {
      try {
        this.#recorder.stop();
      } catch {
        /* already stopped */
      }
    }
    this.#recording = false;
    this.#streaming = false;
    this.#onStreamChunk = null;
    this.#onLevel = null;
    this.#stopLevelMonitor();
    this.#releaseStream();
    this.#recorder = null;
    this.#chunks = [];
  }

  #stopLevelMonitor(): void {
    if (this.#levelRaf != null) {
      cancelAnimationFrame(this.#levelRaf);
      this.#levelRaf = null;
    }
    this.#levelAnalyser = null;
    this.#onLevel = null;
    if (this.#levelContext && this.#levelContext.state !== "closed") {
      void this.#levelContext.close();
    }
    this.#levelContext = null;
  }

  #releaseStream(): void {
    if (this.#stream) {
      for (const track of this.#stream.getTracks()) track.stop();
      this.#stream = null;
    }
  }
}

export function blobToBase64(blob: Blob): Promise<string> {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onloadend = () => {
      const dataUrl = reader.result as string;
      const idx = dataUrl.indexOf(",");
      resolve(idx >= 0 ? dataUrl.slice(idx + 1) : dataUrl);
    };
    reader.onerror = () => reject(new Error("Failed to read audio blob"));
    reader.readAsDataURL(blob);
  });
}
