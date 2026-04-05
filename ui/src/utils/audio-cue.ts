/**
 * Opt-in audio feedback for UI interactions.
 * Disabled by default. Enable via localStorage: hu-audio-cues = 'true'
 * All sounds generated via Web Audio API — zero network requests.
 */

let _sharedCtx: AudioContext | null = null;

function getAudioContext(): AudioContext {
  if (!_sharedCtx) _sharedCtx = new AudioContext();
  return _sharedCtx;
}

export function isAudioEnabled(): boolean {
  try {
    return localStorage.getItem("hu-audio-cues") === "true";
  } catch {
    return false;
  }
}

export function playAudioCue(type: "send" | "receive" | "success" | "error"): void {
  if (!isAudioEnabled()) return;
  if (window.matchMedia?.("(prefers-reduced-motion: reduce)").matches) return;

  try {
    const ctx = getAudioContext();
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.connect(gain);
    gain.connect(ctx.destination);

    const configs: Record<
      string,
      { freq: number; duration: number; wave: OscillatorType; vol: number }
    > = {
      send: { freq: 880, duration: 0.05, wave: "triangle", vol: 0.06 },
      receive: { freq: 660, duration: 0.08, wave: "sine", vol: 0.05 },
      success: { freq: 880, duration: 0.12, wave: "sine", vol: 0.06 },
      error: { freq: 330, duration: 0.1, wave: "sawtooth", vol: 0.04 },
    };

    const cfg = configs[type] ?? configs.send;
    osc.type = cfg.wave;
    osc.frequency.setValueAtTime(cfg.freq, ctx.currentTime);
    if (type === "success") {
      osc.frequency.linearRampToValueAtTime(1320, ctx.currentTime + cfg.duration);
    }
    gain.gain.setValueAtTime(cfg.vol, ctx.currentTime);
    gain.gain.exponentialRampToValueAtTime(0.001, ctx.currentTime + cfg.duration);

    osc.start(ctx.currentTime);
    osc.stop(ctx.currentTime + cfg.duration + 0.01);

    osc.onended = () => {
      try {
        osc.disconnect();
        gain.disconnect();
      } catch {
        /* nodes may already be torn down */
      }
    };
  } catch {
    /* AudioContext not available */
  }
}
