import { describe, it, expect, vi } from "vitest";
import { createVoiceSilenceController, VOICE_STREAM_SILENCE } from "./voice-stream-silence.js";

describe("createVoiceSilenceController", () => {
  it("does not fire if the user never speaks", () => {
    let t = 0;
    const now = () => t;
    const onEnd = vi.fn();
    const c = createVoiceSilenceController({
      isActive: () => true,
      onSilenceEnd: onEnd,
      now,
    });
    c.reset();
    for (let i = 0; i < 50; i++) {
      t += 100;
      c.onLevel(0.001);
    }
    expect(onEnd).not.toHaveBeenCalled();
  });

  it("fires once after speech then sustained silence", () => {
    let t = 10_000;
    const now = () => t;
    const onEnd = vi.fn();
    const c = createVoiceSilenceController({
      isActive: () => true,
      onSilenceEnd: onEnd,
      now,
    });
    c.reset();
    t += VOICE_STREAM_SILENCE.MIN_BEFORE_AUTO_MS + 50;
    c.onLevel(0.001);
    c.onLevel(0.05);
    t += VOICE_STREAM_SILENCE.SILENCE_END_MS + 1;
    c.onLevel(0.001);
    expect(onEnd).toHaveBeenCalledTimes(1);
  });

  it("does not fire again until reset", () => {
    let t = 0;
    const now = () => t;
    const onEnd = vi.fn();
    const c = createVoiceSilenceController({
      isActive: () => true,
      onSilenceEnd: onEnd,
      now,
    });
    c.reset();
    t = 1000;
    t += VOICE_STREAM_SILENCE.MIN_BEFORE_AUTO_MS;
    c.onLevel(0.05);
    t += VOICE_STREAM_SILENCE.SILENCE_END_MS + 1;
    c.onLevel(0.001);
    expect(onEnd).toHaveBeenCalledTimes(1);
    t += 10_000;
    c.onLevel(0.001);
    expect(onEnd).toHaveBeenCalledTimes(1);
  });
});
