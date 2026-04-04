/**
 * Page-wide scroll animations powered by GSAP ScrollTrigger.
 * Lazy-loaded after LCP to avoid blocking initial paint.
 *
 * Replaces hand-rolled IntersectionObserver + rAF tween systems with
 * GSAP's batched ticker for fewer long tasks and smoother choreography.
 */

import { gsap } from "gsap";
import { ScrollTrigger } from "gsap/ScrollTrigger";

gsap.registerPlugin(ScrollTrigger);

if (window.matchMedia("(prefers-reduced-motion: reduce)").matches) {
  ScrollTrigger.config({ limitCallbacks: true });
  gsap.globalTimeline.timeScale(100);
}

// ═══ Scroll reveals ([data-reveal], .glass-enter) ═══

export function initScrollReveals() {
  ScrollTrigger.batch("[data-reveal], .glass-enter", {
    onEnter: (batch) => {
      gsap.to(batch, {
        opacity: 1,
        y: 0,
        stagger: 0.04,
        duration: 0.6,
        ease: "power2.out",
        overwrite: true,
        onComplete() {
          batch.forEach((el) => el.classList.add("revealed"));
        },
      });
    },
    start: "top 85%",
  });
}

// ═══ Count-up numbers ═══

export function initCounters() {
  const numbersGrid = document.querySelector(".numbers-grid");
  if (!numbersGrid) return;

  ScrollTrigger.create({
    trigger: numbersGrid,
    start: "top 85%",
    once: true,
    onEnter() {
      document.querySelectorAll(".number-item").forEach((item, i) => {
        const el = item.querySelector("[data-count]") as HTMLElement;
        if (!el) return;
        const target = parseInt(el.dataset.count || "0");
        const valSpan = el.querySelector(".count-value");
        if (!valSpan) return;

        const obj = { val: 0 };
        gsap.to(obj, {
          val: target,
          duration: 1.5,
          delay: i * 0.08,
          ease: "power2.out",
          snap: { val: 1 },
          onUpdate() {
            valSpan.textContent =
              target > 100
                ? Math.round(obj.val).toLocaleString()
                : Math.round(obj.val).toString();
          },
          onComplete() {
            gsap.to(item, {
              scale: 1.04,
              duration: 0.15,
              ease: "power2.out",
              yoyo: true,
              repeat: 1,
            });
          },
        });
      });
    },
  });

  const dashPreview = document.querySelector(".dash-preview");
  if (!dashPreview) return;

  ScrollTrigger.create({
    trigger: dashPreview,
    start: "top 85%",
    once: true,
    onEnter() {
      gsap.to(dashPreview, { opacity: 1, duration: 0.6, ease: "power2.out" });

      document.querySelectorAll(".dash-count").forEach((el, i) => {
        const target = parseInt((el as HTMLElement).dataset.target || "0");
        const obj = { val: 0 };
        gsap.to(obj, {
          val: target,
          duration: 1.2,
          delay: 0.3 + i * 0.1,
          ease: "power2.out",
          snap: { val: 1 },
          onUpdate() {
            el.textContent = Math.round(obj.val).toString();
          },
        });
      });

      initDashActivity();
    },
  });
}

function initDashActivity() {
  const actContainer = document.getElementById("dash-activity");
  if (!actContainer) return;

  const iconChatCircle =
    '<svg viewBox="0 0 256 256" fill="currentColor" width="16" height="16"><path d="M128,24A104,104,0,0,0,36.18,176.88L24.83,210.93a16,16,0,0,0,20.24,20.24l34.05-11.35A104,104,0,1,0,128,24Zm0,192a87.87,87.87,0,0,1-44.06-11.81,8,8,0,0,0-4-1.08,8.09,8.09,0,0,0-2.53.41L40,216l12.47-37.4a8,8,0,0,0-.66-6.54A88,88,0,1,1,128,216Z"/></svg>';
  const iconLightning =
    '<svg viewBox="0 0 256 256" fill="currentColor" width="16" height="16"><path d="M215.79,118.17a8,8,0,0,0-5-5.66L153.18,90.9l14.66-73.33a8,8,0,0,0-13.69-7l-112,120a8,8,0,0,0,3,12.95l57.63,21.61L88.16,238.43a8,8,0,0,0,13.69,7l112-120A8,8,0,0,0,215.79,118.17Z"/></svg>';
  const iconPlayCircle =
    '<svg viewBox="0 0 256 256" fill="currentColor" width="16" height="16"><path d="M128,24A104,104,0,1,0,232,128,104.11,104.11,0,0,0,128,24Zm0,192a88,88,0,1,1,88-88A88.1,88.1,0,0,1,128,216Zm36.44-94.66-48-32A8,8,0,0,0,104,96v64a8,8,0,0,0,12.44,6.66l48-32a8,8,0,0,0,0-13.32ZM120,145.05V111l25.58,17Z"/></svg>';

  const activities = [
    {
      icon: iconChatCircle,
      text: "Alice via Telegram: Can you review the PR?",
      time: "just now",
    },
    { icon: iconLightning, text: "Tool shell: git status", time: "1m ago" },
    {
      icon: iconChatCircle,
      text: "Bob via Discord: Deploy looks good",
      time: "2m ago",
    },
    {
      icon: iconPlayCircle,
      text: 'Session "Code Review" started',
      time: "3m ago",
    },
  ];

  const tl = gsap.timeline({ delay: 0.8 });
  activities.forEach((act) => {
    const div = document.createElement("div");
    div.className = "flex items-center gap-2";
    div.innerHTML = `<span class="flex-shrink-0 [&>svg]:block">${act.icon}</span><span style="color: var(--hu-text-secondary); flex: 1;">${act.text}</span><span class="text-xs" style="color: var(--hu-text-muted)">${act.time}</span>`;
    gsap.set(div, { opacity: 0 });
    actContainer.appendChild(div);
    tl.to(div, { opacity: 1, duration: 0.4, ease: "power2.out" }, "+=0.25");
  });
}

// ═══ Terminal typewriter ═══

interface TermLine {
  prompt: boolean;
  text: string;
  highlights?: Array<{ text: string; cls: string }>;
}

export function initTerminal() {
  const terminalEl = document.getElementById("terminal");
  const outputEl = document.getElementById("terminal-output");
  if (!terminalEl || !outputEl) return;

  ScrollTrigger.create({
    trigger: terminalEl,
    start: "top 85%",
    once: true,
    onEnter() {
      gsap.to(terminalEl, { opacity: 1, duration: 0.6, ease: "power2.out" });
      typeTerminal(outputEl);
    },
  });
}

function typeTerminal(container: HTMLElement) {
  const accentColor = "var(--color-accent)";
  const lines: TermLine[] = [
    {
      prompt: true,
      text: "git clone https://github.com/sethdford/h-uman.git",
    },
    { prompt: true, text: "cd h-uman && mkdir -p build && cd build" },
    {
      prompt: true,
      text: "cmake .. -DCMAKE_BUILD_TYPE=MinSizeRel -DHU_ENABLE_LTO=ON",
    },
    { prompt: true, text: "cmake --build ." },
    { prompt: false, text: "[100%] Built target human" },
    { prompt: true, text: "ls -lh human" },
    {
      prompt: false,
      text: "-rwxr-xr-x  1 user  staff  1750K  human",
      highlights: [{ text: "1750K", cls: accentColor }],
    },
    {
      prompt: true,
      text: './human agent -m "Hello from a ~1.7 MB binary."',
    },
    {
      prompt: false,
      text: "Hello! I'm h-uman — a fully autonomous AI assistant",
      highlights: [{ text: "h-uman", cls: accentColor }],
    },
    {
      prompt: false,
      text: "running in 1,750 kilobytes. How can I help?",
    },
  ];

  const tl = gsap.timeline();
  let cumulativeDelay = 0;

  lines.forEach((line) => {
    const div = document.createElement("div");
    div.className = "terminal-line";

    if (line.prompt) {
      const p = document.createElement("span");
      p.style.color = "var(--color-accent)";
      p.textContent = "$ ";
      div.appendChild(p);
      const textSpan = document.createElement("span");
      textSpan.style.color = "var(--hu-text)";
      div.appendChild(textSpan);
      container.appendChild(div);

      const charDuration = line.text.length * 0.018;
      const proxy = { length: 0 };
      tl.to(
        proxy,
        {
          length: line.text.length,
          duration: charDuration,
          ease: "none",
          snap: { length: 1 },
          onUpdate() {
            textSpan.textContent = line.text.slice(0, Math.round(proxy.length));
          },
        },
        cumulativeDelay,
      );
      cumulativeDelay += charDuration + 0.3;
    } else {
      const w = document.createElement("span");
      w.style.color = "var(--hu-text-muted)";
      if (line.highlights?.length) {
        let rem = line.text;
        for (const hl of line.highlights) {
          const idx = rem.indexOf(hl.text);
          if (idx >= 0) {
            if (idx > 0)
              w.appendChild(document.createTextNode(rem.slice(0, idx)));
            const s = document.createElement("span");
            s.style.color = hl.cls;
            s.textContent = hl.text;
            w.appendChild(s);
            rem = rem.slice(idx + hl.text.length);
          }
        }
        if (rem) w.appendChild(document.createTextNode(rem));
      } else {
        w.textContent = line.text;
      }
      div.appendChild(w);
      gsap.set(div, { opacity: 0 });
      container.appendChild(div);
      tl.to(
        div,
        { opacity: 1, duration: 0.3, ease: "power2.out" },
        cumulativeDelay,
      );
      cumulativeDelay += 0.2;
    }
  });

  tl.call(() => {
    const cursor = document.createElement("span");
    cursor.className = "terminal-cursor";
    container.lastElementChild?.appendChild(cursor);
  });
}

// ═══ Architecture SVG line drawing ═══

export function initArchitecture() {
  const archDiagram = document.getElementById("arch-diagram");
  if (!archDiagram) return;

  ScrollTrigger.create({
    trigger: archDiagram,
    start: "top 85%",
    once: true,
    onEnter() {
      const core = document.getElementById("arch-core");
      if (core)
        gsap.to(core, { opacity: 1, duration: 0.6, ease: "power2.out" });

      gsap.utils.toArray<SVGElement>(".arch-line").forEach((line, i) => {
        line.setAttribute("stroke-dasharray", "200");
        line.setAttribute("stroke-dashoffset", "200");
        gsap.to(line, {
          attr: { "stroke-dashoffset": 0 },
          duration: 0.7,
          delay: 0.3 + i * 0.06,
          ease: "power2.out",
          onComplete() {
            line.setAttribute("stroke", "var(--color-accent)");
          },
        });
      });

      gsap.utils.toArray<HTMLElement>(".arch-node").forEach((node, i) => {
        gsap.to(node, {
          opacity: 1,
          duration: 0.5,
          delay: 0.6 + i * 0.08,
          ease: "power2.out",
        });
      });
    },
  });

  document.querySelectorAll(".arch-node").forEach((node) => {
    const highlight = () => {
      const c = node.querySelector("circle");
      if (c) c.setAttribute("stroke", "var(--color-accent)");
    };
    const unhighlight = () => {
      const c = node.querySelector("circle");
      if (c) c.setAttribute("stroke", "var(--color-node-stroke)");
    };
    node.addEventListener("mouseenter", highlight);
    node.addEventListener("mouseleave", unhighlight);
    node.addEventListener("focusin", highlight);
    node.addEventListener("focusout", unhighlight);
  });
}

// ═══ Comparison bars ═══

export function initComparisonBars() {
  const barSection = document.querySelector(".comparison-bars");
  if (!barSection) return;

  ScrollTrigger.create({
    trigger: barSection,
    start: "top 85%",
    once: true,
    onEnter() {
      gsap.utils.toArray<HTMLElement>(".comparison-bar").forEach((bar, i) => {
        gsap.fromTo(
          bar,
          { scaleX: 0 },
          {
            scaleX: 1,
            duration: 0.6,
            delay: 0.1 + i * 0.06,
            ease: "power2.out",
          },
        );
      });
    },
  });
}

// ═══ Crystal grid ═══

export function initCrystalGrid() {
  const crystalGrid = document.getElementById("crystalGrid");
  if (!crystalGrid) return;

  const modules = [
    { name: "core", core: true },
    { name: "json", core: true },
    { name: "http", core: true },
    { name: "str", core: true },
    { name: "arena", core: true },
    { name: "err", core: true },
    { name: "agent" },
    { name: "ctx" },
    { name: "plan" },
    { name: "disp" },
    { name: "comp" },
    { name: "turn" },
    { name: "prov" },
    { name: "chan" },
    { name: "tool" },
    { name: "mem" },
    { name: "sec" },
    { name: "run" },
    { name: "sse" },
    { name: "ws" },
    { name: "log" },
    { name: "cfg" },
    { name: "gw" },
    { name: "cli" },
    { name: "pers" },
    { name: "emb" },
    { name: "sql" },
    { name: "lru" },
    { name: "vec" },
    { name: "obs" },
    {},
    {},
    {},
    {},
    {},
    {},
  ];

  modules.forEach((m, i) => {
    const cell = document.createElement("div");
    cell.className =
      "crystal-cell" + (m.core ? " core" : m.name ? " filled" : "");
    cell.textContent = m.name || "";
    cell.style.transitionDelay = i * 30 + "ms";
    crystalGrid.appendChild(cell);
  });

  ScrollTrigger.create({
    trigger: crystalGrid,
    start: "top 70%",
    once: true,
    onEnter() {
      crystalGrid
        .querySelectorAll(".crystal-cell")
        .forEach((c) => c.classList.add("on"));
    },
  });
}

// ═══ Device spectrum line ═══

export function initDeviceSpectrum() {
  const deviceLine = document.getElementById("deviceLine");
  if (!deviceLine) return;

  ScrollTrigger.create({
    trigger: deviceLine,
    start: "top 50%",
    once: true,
    onEnter() {
      deviceLine.classList.add("on");
      deviceLine.querySelectorAll(".spectrum-dot").forEach((dot, i) => {
        gsap.delayedCall(0.5 + i * 0.2, () => dot.classList.add("on"));
      });
    },
  });
}

// ═══ Dependency tree ═══

export function initDepTree() {
  const depTree = document.getElementById("depTree");
  if (!depTree) return;

  ScrollTrigger.create({
    trigger: depTree,
    start: "top 80%",
    once: true,
    onEnter() {
      depTree.querySelectorAll("[data-delay]").forEach((el) => {
        const delay = parseInt((el as HTMLElement).dataset.delay || "0") / 1000;
        gsap.delayedCall(delay, () => el.classList.add("on"));
      });
    },
  });
}

// ═══ Quality rings ═══

export function initQualityRings() {
  const qCards = document.querySelectorAll(".q-card");
  if (!qCards.length) return;

  ScrollTrigger.batch(".q-card", {
    onEnter: (batch) => {
      batch.forEach((card) => {
        card.querySelectorAll(".ring-fg").forEach((r) => r.classList.add("on"));
      });
    },
    start: "top 70%",
  });
}

// ═══ Nav glass toggle ═══

export function initNavGlass() {
  const nav = document.getElementById("site-nav");
  if (!nav) return;

  ScrollTrigger.create({
    start: 80,
    onUpdate(self) {
      if (self.direction === 1 && window.scrollY > 80) {
        nav.classList.add("glass-nav");
      } else if (window.scrollY <= 80) {
        nav.classList.remove("glass-nav");
      }
    },
  });

  if (window.scrollY > 80) nav.classList.add("glass-nav");
}

// ═══ Bootstrap: initialize all animations ═══

export function initAll() {
  initNavGlass();
  initScrollReveals();
  initCounters();
  initTerminal();
  initArchitecture();
  initComparisonBars();
  initCrystalGrid();
  initDeviceSpectrum();
  initDepTree();
  initQualityRings();
}
