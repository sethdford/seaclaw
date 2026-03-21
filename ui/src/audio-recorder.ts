/**
 * Shared audio recording utility using MediaRecorder API.
 * Works across Chrome, Firefox, Safari (14.5+), and Edge.
 * Replaces the browser-specific Web Speech API for voice capture.
 */

export interface AudioCaptureResult {
  blob: Blob;
  mimeType: string;
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

  get isRecording(): boolean {
    return this.#recording;
  }

  get isSupported(): boolean {
    return (
      typeof navigator !== "undefined" &&
      typeof navigator.mediaDevices !== "undefined" &&
      typeof MediaRecorder !== "undefined" &&
      selectMimeType() !== ""
    );
  }

  /**
   * Stream encoded media chunks (~250 ms) to the callback while recording.
   * Stops the mic only when {@link stopStreaming} is called.
   */
  async startStreaming(onChunk: (data: ArrayBuffer) => void): Promise<void> {
    if (this.#recording) return;

    const mimeType = selectMimeType();
    if (!mimeType) throw new Error("No supported audio MIME type found");

    const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
    this.#stream = stream;
    this.#mimeType = mimeType;
    this.#streaming = true;
    this.#onStreamChunk = onChunk;

    const recorder = new MediaRecorder(stream, { mimeType });
    recorder.ondataavailable = (e: BlobEvent) => {
      if (e.data.size === 0 || !this.#onStreamChunk) return;
      void e.data.arrayBuffer().then((ab) => this.#onStreamChunk?.(ab));
    };
    recorder.start(250);
    this.#recorder = recorder;
    this.#recording = true;
  }

  async stopStreaming(): Promise<void> {
    if (!this.#recorder || !this.#recording || !this.#streaming) {
      throw new Error("Not streaming");
    }
    return new Promise((resolve, reject) => {
      const recorder = this.#recorder!;
      recorder.onstop = () => {
        this.#streaming = false;
        this.#onStreamChunk = null;
        this.#recording = false;
        this.#releaseStream();
        this.#recorder = null;
        resolve();
      };
      recorder.onerror = () => {
        this.#streaming = false;
        this.#onStreamChunk = null;
        this.#recording = false;
        this.#releaseStream();
        this.#recorder = null;
        reject(new Error("Streaming recording failed"));
      };
      try {
        recorder.stop();
      } catch {
        this.#streaming = false;
        this.#onStreamChunk = null;
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
    this.#releaseStream();
    this.#recorder = null;
    this.#chunks = [];
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
