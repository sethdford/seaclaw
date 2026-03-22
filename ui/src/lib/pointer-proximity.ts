/**
 * Pointer proximity for Quiet Mastery: per-element `--hu-proximity`, pointer offsets,
 * and optional magnetic nudge. Complements `dynamic-light.ts` (document-level light).
 *
 * Tunables below are in px or unitless factors; when adjusting visuals, prefer
 * aligning perceived scale with `--hu-space-*`, `--hu-touch-target-min`, and
 * motion tokens in `docs/standards/design/motion-design.md`.
 */

/** Falloff radius (px); ~12–13× typical `--hu-space-md` for a soft hover halo. */
const PROXIMITY_RADIUS = 200;

/** Max observed elements; keeps rAF work bounded (Quiet Mastery perf budget). */
const MAX_TRACKED = 20;

/** Magnetic pull scale (px at full proximity); keep subtle vs `--hu-duration-*` transitions. */
const MAGNETIC_STRENGTH = 8;

/** Max bbox dimension (px) to qualify for magnetic offset; ~`--hu-touch-target-min` band. */
const MAGNETIC_THRESHOLD = 48;

function prefersReducedMotion(): boolean {
  return window.matchMedia("(prefers-reduced-motion: reduce)").matches;
}

function isTouchDevice(): boolean {
  return window.matchMedia("(hover: none)").matches;
}

export class PointerProximity {
  private elements = new Set<HTMLElement>();
  private visibleElements = new Set<HTMLElement>();
  private pointerX = 0;
  private pointerY = 0;
  private rafId: number | null = null;
  private observer: IntersectionObserver | null = null;
  private started = false;

  private boundPointerMove = this.handlePointerMove.bind(this);
  private boundTick = this.tick.bind(this);

  start(): void {
    if (this.started) return;
    if (prefersReducedMotion() || isTouchDevice()) return;

    this.started = true;

    this.observer = new IntersectionObserver(
      (entries) => {
        for (const entry of entries) {
          const el = entry.target as HTMLElement;
          if (entry.isIntersecting) {
            this.visibleElements.add(el);
          } else {
            this.visibleElements.delete(el);
            this.clearProperties(el);
          }
        }
      },
      { threshold: 0 },
    );

    for (const el of this.elements) {
      this.observer.observe(el);
    }

    window.addEventListener("pointermove", this.boundPointerMove, { passive: true });
    this.rafId = requestAnimationFrame(this.boundTick);
  }

  stop(): void {
    this.started = false;
    window.removeEventListener("pointermove", this.boundPointerMove);

    if (this.rafId !== null) {
      cancelAnimationFrame(this.rafId);
      this.rafId = null;
    }

    if (this.observer) {
      this.observer.disconnect();
      this.observer = null;
    }

    for (const el of this.elements) {
      this.clearProperties(el);
    }
    this.visibleElements.clear();
  }

  observe(element: HTMLElement): void {
    if (this.elements.has(element)) return;
    if (this.elements.size >= MAX_TRACKED) return;
    this.elements.add(element);
    if (this.started && this.observer) {
      this.observer.observe(element);
    }
  }

  unobserve(element: HTMLElement): void {
    this.elements.delete(element);
    this.visibleElements.delete(element);
    if (this.observer) {
      this.observer.unobserve(element);
    }
    this.clearProperties(element);
  }

  private handlePointerMove(e: PointerEvent): void {
    this.pointerX = e.clientX;
    this.pointerY = e.clientY;
  }

  private tick(): void {
    for (const el of this.visibleElements) {
      this.updateElement(el);
    }
    this.rafId = requestAnimationFrame(this.boundTick);
  }

  private updateElement(el: HTMLElement): void {
    const rect = el.getBoundingClientRect();
    const centerX = rect.left + rect.width / 2;
    const centerY = rect.top + rect.height / 2;
    const dx = this.pointerX - centerX;
    const dy = this.pointerY - centerY;
    const distance = Math.sqrt(dx * dx + dy * dy);
    const proximity = Math.max(0, 1 - distance / PROXIMITY_RADIUS);

    el.style.setProperty("--hu-proximity", proximity.toFixed(3));
    el.style.setProperty("--hu-pointer-x", `${dx.toFixed(1)}px`);
    el.style.setProperty("--hu-pointer-y", `${dy.toFixed(1)}px`);

    if (proximity > 0 && Math.max(rect.width, rect.height) <= MAGNETIC_THRESHOLD) {
      const pull = proximity * MAGNETIC_STRENGTH;
      const angle = Math.atan2(dy, dx);
      el.style.setProperty("--hu-magnetic-x", `${(Math.cos(angle) * pull * -1).toFixed(1)}px`);
      el.style.setProperty("--hu-magnetic-y", `${(Math.sin(angle) * pull * -1).toFixed(1)}px`);
    } else {
      el.style.removeProperty("--hu-magnetic-x");
      el.style.removeProperty("--hu-magnetic-y");
    }
  }

  private clearProperties(el: HTMLElement): void {
    el.style.removeProperty("--hu-proximity");
    el.style.removeProperty("--hu-pointer-x");
    el.style.removeProperty("--hu-pointer-y");
    el.style.removeProperty("--hu-magnetic-x");
    el.style.removeProperty("--hu-magnetic-y");
  }
}

export const pointerProximity = new PointerProximity();
