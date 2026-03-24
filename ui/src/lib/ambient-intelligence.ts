const WARMTH_UPDATE_INTERVAL = 300_000; // 5 minutes

function prefersReducedMotion(): boolean {
  return window.matchMedia("(prefers-reduced-motion: reduce)").matches;
}

/** Sepia driver 0–0.12 — dawn/dusk, twilight bands, neutral day (dashboard). */
function getTimeWarmth(): number {
  const hour = new Date().getHours();
  if (hour < 7 || hour >= 19) return 0.12;
  if ((hour >= 7 && hour < 10) || (hour >= 17 && hour < 19)) return 0.06;
  return 0;
}

export class AmbientIntelligence {
  private intervalId: ReturnType<typeof setInterval> | null = null;
  private started = false;
  private target: HTMLElement = document.documentElement;

  start(host?: HTMLElement): void {
    if (this.started) return;
    if (prefersReducedMotion()) return;

    this.target = host ?? document.documentElement;
    this.started = true;
    this.updateWarmth();
    this.intervalId = setInterval(() => this.updateWarmth(), WARMTH_UPDATE_INTERVAL);
  }

  stop(): void {
    this.started = false;
    if (this.intervalId !== null) {
      clearInterval(this.intervalId);
      this.intervalId = null;
    }
    this.target.style.removeProperty("--hu-ambient-warmth");
    this.target = document.documentElement;
  }

  private updateWarmth(): void {
    const warmth = getTimeWarmth();
    this.target.style.setProperty("--hu-ambient-warmth", warmth.toFixed(2));
  }
}

export const ambientIntelligence = new AmbientIntelligence();
