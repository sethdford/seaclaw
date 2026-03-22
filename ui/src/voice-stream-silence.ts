/**
 * Shared end-of-utterance detection for gateway voice streaming (mic level → silence).
 */

export const VOICE_STREAM_SILENCE = {
  /** RMS above this counts as speech (AnalyserNode time-domain). */
  RMS_SPEECH: 0.018,
  /** After speech, end capture when quiet for this long (ms). */
  SILENCE_END_MS: 1400,
  /** Do not auto-end before this much capture time (ms). */
  MIN_BEFORE_AUTO_MS: 700,
} as const;

export type VoiceSilenceController = {
  /** Call when starting a new streaming take. */
  reset: () => void;
  /** Pass to AudioRecorder.startStreaming({ onLevel }). */
  onLevel: (rms: number) => void;
};

/**
 * Fires `onSilenceEnd` once per take when silence follows speech (see constants).
 * Caller should ignore further callbacks until the next `reset()` (e.g. after stop).
 */
export function createVoiceSilenceController(options: {
  isActive: () => boolean;
  onSilenceEnd: () => void;
  /** Override for tests */
  now?: () => number;
}): VoiceSilenceController {
  let heard = false;
  let lastVoice = 0;
  let started = 0;
  let fired = false;
  const now = options.now ?? (() => performance.now());

  return {
    reset(): void {
      heard = false;
      const t = now();
      lastVoice = t;
      started = t;
      fired = false;
    },
    onLevel(rms: number): void {
      if (!options.isActive() || fired) return;
      const t = now();
      if (rms >= VOICE_STREAM_SILENCE.RMS_SPEECH) {
        heard = true;
        lastVoice = t;
        return;
      }
      if (
        heard &&
        t - lastVoice >= VOICE_STREAM_SILENCE.SILENCE_END_MS &&
        t - started >= VOICE_STREAM_SILENCE.MIN_BEFORE_AUTO_MS
      ) {
        fired = true;
        options.onSilenceEnd();
      }
    },
  };
}
