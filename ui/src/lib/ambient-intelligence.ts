const WARMTH_UPDATE_INTERVAL = 300_000; // 5 minutes
const WARMTH_MIX_MAX = 0.03;

function prefersReducedMotion(): boolean {
  return window.matchMedia("(prefers-reduced-motion: reduce)").matches;
}

function isMobile(): boolean {
  return window.matchMedia("(max-width: 768px)").matches; /* --hu-breakpoint-md */
}

function getTimeWarmth(): number {
  const hour = new Date().getHours();
  if (hour >= 6 && hour < 10) return 0.0; // cool morning
  if (hour >= 10 && hour < 16) return 0.5; // neutral day
  if (hour >= 16 && hour < 22) return 1.0; // warm evening
  return 0.3; // slightly cool night
}

export class AmbientIntelligence {
  private intervalId: ReturnType<typeof setInterval> | null = null;
  private started = false;

  start(): void {
    if (this.started) return;
    if (prefersReducedMotion() || isMobile()) return;

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
    const root = document.documentElement;
    root.style.removeProperty("--hu-ambient-warmth");
    root.style.removeProperty("--hu-ambient-warmth-mix");
  }

  private updateWarmth(): void {
    const warmth = getTimeWarmth();
    const root = document.documentElement;
    root.style.setProperty("--hu-ambient-warmth", warmth.toFixed(2));
    root.style.setProperty(
      "--hu-ambient-warmth-mix",
      `${(warmth * WARMTH_MIX_MAX * 100).toFixed(1)}%`,
    );
  }
}

export const ambientIntelligence = new AmbientIntelligence();
