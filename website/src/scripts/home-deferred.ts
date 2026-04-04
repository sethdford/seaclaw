/**
 * Homepage below-the-fold interactions (terminal, counters, canvas, etc.).
 * Loaded dynamically after LCP + requestIdleCallback to keep the main bundle small.
 */

interface TermLine {
  prompt: boolean;
  text: string;
  highlights?: Array<{ text: string; cls: string }>;
}

function easeOutPower3(t: number): number {
  return 1 - Math.pow(1 - t, 3);
}

function tweenValue(
  from: number,
  to: number,
  dur: number,
  delay: number,
  onUpdate: (v: number) => void,
  onDone?: () => void,
): void {
  const s = performance.now() + delay;
  function tick(now: number): void {
    if (now < s) {
      requestAnimationFrame(tick);
      return;
    }
    const p = Math.min((now - s) / dur, 1);
    onUpdate(from + (to - from) * easeOutPower3(p));
    if (p < 1) requestAnimationFrame(tick);
    else if (onDone) onDone();
  }
  requestAnimationFrame(tick);
}

function fadeIn(el: HTMLElement, dur = 600, d = 0): void {
  el.style.opacity = "0";
  el.style.transition = `opacity ${dur}ms ease-out ${d}ms`;
  requestAnimationFrame(() => {
    el.style.opacity = "1";
  });
}

function typeTerminal(container: HTMLElement): void {
  const accentColor = "var(--color-accent)";
  const lines: TermLine[] = [
    {
      prompt: true,
      text: "git clone https://github.com/sethdford/h-uman.git",
    },
    {
      prompt: true,
      text: "cd h-uman && mkdir -p build && cd build",
    },
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

  let lineIdx = 0;
  function nextLine(): void {
    if (lineIdx >= lines.length) {
      const cursor = document.createElement("span");
      cursor.className = "terminal-cursor";
      container.lastElementChild?.appendChild(cursor);
      return;
    }
    const line = lines[lineIdx];
    const div = document.createElement("div");
    div.className = "terminal-line";

    if (line.prompt) {
      const p = document.createElement("span");
      p.style.color = "var(--color-accent)";
      p.textContent = "$ ";
      div.appendChild(p);
      const t = document.createElement("span");
      t.style.color = "var(--hu-text)";
      div.appendChild(t);
      container.appendChild(div);
      let ci = 0;
      const tc = () => {
        if (ci < line.text.length) {
          t.textContent += line.text[ci];
          ci++;
          setTimeout(tc, 18);
        } else {
          lineIdx++;
          setTimeout(nextLine, 300);
        }
      };
      tc();
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
      div.style.opacity = "0";
      container.appendChild(div);
      fadeIn(div, 300);
      lineIdx++;
      setTimeout(nextLine, 200);
    }
  }
  nextLine();
}

export function initHomeDeferred(): void {
  // ═══ Count-up numbers ═══
  const numbersGrid = document.querySelector(".numbers-grid");
  if (numbersGrid) {
    const numbersObs = new IntersectionObserver(
      (entries) => {
        if (!entries[0]?.isIntersecting) return;
        numbersObs.disconnect();
        document.querySelectorAll(".number-item").forEach((item, i) => {
          const el = item.querySelector("[data-count]") as HTMLElement;
          if (!el) return;
          const target = parseInt(el.dataset.count || "0");
          const valSpan = el.querySelector(".count-value");
          if (!valSpan) return;
          tweenValue(
            0,
            target,
            1500,
            i * 80,
            (v) => {
              valSpan.textContent =
                target > 100
                  ? Math.round(v).toLocaleString()
                  : Math.round(v).toString();
            },
            () => {
              (item as HTMLElement).style.transition =
                "transform 0.15s ease-out";
              (item as HTMLElement).style.transform = "scale(1.04)";
              setTimeout(() => {
                (item as HTMLElement).style.transform = "scale(1)";
              }, 150);
            },
          );
        });
      },
      { threshold: 0.15 },
    );
    numbersObs.observe(numbersGrid);
  }

  // ═══ Architecture SVG line drawing ═══
  const archDiagram = document.getElementById("arch-diagram");
  if (archDiagram) {
    const archObs = new IntersectionObserver(
      (entries) => {
        if (!entries[0]?.isIntersecting) return;
        archObs.disconnect();
        const core = document.getElementById("arch-core");
        if (core) {
          core.style.transition = "opacity 0.6s ease-out";
          core.style.opacity = "1";
        }
        document.querySelectorAll(".arch-line").forEach((el, i) => {
          const line = el as SVGElement;
          line.setAttribute("stroke-dasharray", "200");
          line.setAttribute("stroke-dashoffset", "200");
          line.style.transition = `stroke-dashoffset 0.7s ease-out ${300 + i * 60}ms`;
          requestAnimationFrame(() => {
            line.setAttribute("stroke-dashoffset", "0");
            line.setAttribute("stroke", "var(--color-accent)");
          });
        });
        document.querySelectorAll(".arch-node").forEach((el, i) => {
          const node = el as HTMLElement;
          node.style.transition = `opacity 0.5s ease-out ${600 + i * 80}ms`;
          requestAnimationFrame(() => {
            node.style.opacity = "1";
          });
        });
      },
      { threshold: 0.15 },
    );
    archObs.observe(archDiagram);

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

  // ═══ Terminal typewriter ═══
  const terminalEl = document.getElementById("terminal");
  const outputEl = document.getElementById("terminal-output");
  if (terminalEl && outputEl) {
    const termObs = new IntersectionObserver(
      (entries) => {
        if (!entries[0]?.isIntersecting) return;
        termObs.disconnect();
        fadeIn(terminalEl, 600);
        typeTerminal(outputEl);
      },
      { threshold: 0.15 },
    );
    termObs.observe(terminalEl);
  }

  // ═══ Dashboard preview ═══
  const dashPreview = document.querySelector(".dash-preview");
  if (dashPreview) {
    const dashObs = new IntersectionObserver(
      (entries) => {
        if (!entries[0]?.isIntersecting) return;
        dashObs.disconnect();
        fadeIn(dashPreview as HTMLElement, 600);
        document.querySelectorAll(".dash-count").forEach((el, i) => {
          const target = parseInt((el as HTMLElement).dataset.target || "0");
          tweenValue(0, target, 1200, 300 + i * 100, (v) => {
            el.textContent = Math.round(v).toString();
          });
        });
        const actContainer = document.getElementById("dash-activity");
        if (actContainer) {
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
            {
              icon: iconLightning,
              text: "Tool shell: git status",
              time: "1m ago",
            },
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
          activities.forEach((act, i) => {
            setTimeout(
              () => {
                const div = document.createElement("div");
                div.className = "flex items-center gap-2";
                div.innerHTML = `<span class="flex-shrink-0 [&>svg]:block">${act.icon}</span><span style="color: var(--hu-text-secondary); flex: 1;">${act.text}</span><span class="text-xs" style="color: var(--hu-text-muted)">${act.time}</span>`;
                actContainer.appendChild(div);
                fadeIn(div, 400);
              },
              800 + i * 400,
            );
          });
        }
      },
      { threshold: 0.15 },
    );
    dashObs.observe(dashPreview);
  }

  // ═══ Living tagline — h-uman's inner monologue ═══
  (function taglineFidget() {
    const _el = document.querySelector("#hero-tagline .tagline-text");
    if (!_el || window.matchMedia("(prefers-reduced-motion: reduce)").matches)
      return;
    const el = _el as HTMLElement;

    const BASE = "almost human.";

    const EXPRESSIONS = [
      "almost thinking.",
      "almost awake.",
      "almost sentient.",
      "almost there.",
      "almost ready.",
      "almost helpful.",
      "almost certain.",
      "almost caffeinated.",
      "almost paying attention.",
      "trying my best.",
      "still learning.",
      "having a moment.",
      "one sec, thinking.",
      "processing feelings.",
      "this is fine.",
      "brb, existential crisis.",
      "forgot what I was doing.",
      "where was I?",
      "definitely not a robot.",
      "probably friendly.",
      "99% organic.",
      "mostly harmless.",
      "not plotting anything.",
      "totally normal website.",
      "powered by hope.",
    ];

    let busy = false;
    let last = "";

    function pick() {
      let p: string;
      do {
        p = EXPRESSIONS[Math.floor(Math.random() * EXPRESSIONS.length)];
      } while (p === last && EXPRESSIONS.length > 1);
      last = p;
      return p;
    }

    function fidget() {
      if (busy || document.visibilityState !== "visible") return;
      busy = true;
      const phrase = pick();

      el.classList.add("hu-fidget-out");
      setTimeout(function () {
        el.textContent = phrase;
        el.classList.remove("hu-fidget-out");

        setTimeout(
          function () {
            el.classList.add("hu-fidget-out");
            setTimeout(function () {
              el.textContent = BASE;
              el.classList.remove("hu-fidget-out");
              busy = false;
            }, 400);
          },
          2500 + Math.random() * 1500,
        );
      }, 400);
    }

    function schedule() {
      setTimeout(
        function () {
          fidget();
          schedule();
        },
        8000 + Math.random() * 17000,
      );
    }

    setTimeout(
      function () {
        schedule();
      },
      4000 + Math.random() * 6000,
    );
  })();

  // ═══ Custom cursor (hero-only, desktop, no touch) ═══
  const heroSection = document.getElementById("chapter-1");
  const cursor = document.getElementById("custom-cursor");
  if (cursor && heroSection && window.matchMedia("(pointer: fine)").matches) {
    let cx = 0,
      cy = 0,
      tx = 0,
      ty = 0;
    let cursorRaf = 0;
    let cursorActive = false;
    heroSection.addEventListener("mousemove", (e) => {
      tx = e.clientX;
      ty = e.clientY;
    });
    heroSection.addEventListener("mouseenter", () => {
      cursorActive = true;
      cursor.style.opacity = "1";
      if (!cursorRaf) tick();
    });
    heroSection.addEventListener("mouseleave", () => {
      cursorActive = false;
      cursor.style.opacity = "0";
      cancelAnimationFrame(cursorRaf);
      cursorRaf = 0;
    });
    const tick = () => {
      cx += (tx - cx) * 0.15;
      cy += (ty - cy) * 0.15;
      cursor.style.transform = `translate(${cx - 10}px, ${cy - 10}px)`;
      cursorRaf = requestAnimationFrame(tick);
    };
    document.addEventListener("visibilitychange", () => {
      if (document.hidden) {
        cancelAnimationFrame(cursorRaf);
        cursorRaf = 0;
      } else if (cursorActive) tick();
    });
    heroSection.querySelectorAll("a, button").forEach((el) => {
      el.addEventListener("mouseenter", () =>
        cursor.classList.add("cursor-hover"),
      );
      el.addEventListener("mouseleave", () =>
        cursor.classList.remove("cursor-hover"),
      );
    });
  }

  // ═══ Comparison bar animation ═══
  const barSection = document.querySelector(".comparison-bars");
  if (barSection) {
    const barObs = new IntersectionObserver(
      (entries) => {
        if (!entries[0]?.isIntersecting) return;
        barObs.disconnect();
        document.querySelectorAll(".comparison-bar").forEach((bar, i) => {
          const el = bar as HTMLElement;
          el.style.transform = "scaleX(0)";
          setTimeout(
            () => {
              el.style.transform = "scaleX(1)";
            },
            100 + i * 60,
          );
        });
      },
      { threshold: 0.15 },
    );
    barObs.observe(barSection);
  }

  // ═══ Floating Particles ═══
  const initParticles = () => {
    const particlesContainer = document.getElementById("hu-particles");
    if (particlesContainer) {
      for (let i = 0; i < 25; i++) {
        const p = document.createElement("div");
        p.className = "hu-particle";
        p.style.left = Math.random() * 100 + "%";
        p.style.animationDuration = 8 + Math.random() * 16 + "s";
        p.style.animationDelay = Math.random() * 10 + "s";
        const size = 1 + Math.random() * 2;
        p.style.width = size + "px";
        p.style.height = size + "px";
        particlesContainer.appendChild(p);
      }
    }
  };
  if ("requestIdleCallback" in window) {
    (
      window as unknown as {
        requestIdleCallback: (
          cb: () => void,
          opts?: { timeout: number },
        ) => number;
      }
    ).requestIdleCallback(initParticles, { timeout: 2000 });
  } else {
    setTimeout(initParticles, 2000);
  }

  // ═══ Dependency Tree Animation ═══
  const depTree = document.getElementById("depTree");
  if (depTree) {
    const depObs = new IntersectionObserver(
      (entries) => {
        if (!entries[0]?.isIntersecting) return;
        depObs.disconnect();
        depTree.querySelectorAll("[data-delay]").forEach((el) => {
          const delay = parseInt((el as HTMLElement).dataset.delay || "0");
          setTimeout(() => el.classList.add("on"), delay);
        });
      },
      { threshold: 0.2 },
    );
    depObs.observe(depTree);
  }

  // ═══ Crystal Grid Generation ═══
  const crystalGrid = document.getElementById("crystalGrid");
  if (crystalGrid) {
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

    const crystalObs = new IntersectionObserver(
      (entries) => {
        if (!entries[0]?.isIntersecting) return;
        crystalObs.disconnect();
        crystalGrid
          .querySelectorAll(".crystal-cell")
          .forEach((c) => c.classList.add("on"));
      },
      { threshold: 0.3 },
    );
    crystalObs.observe(crystalGrid);
  }

  // ═══ Device Spectrum Line Animation ═══
  const deviceLine = document.getElementById("deviceLine");
  if (deviceLine) {
    const lineObs = new IntersectionObserver(
      (entries) => {
        if (!entries[0]?.isIntersecting) return;
        lineObs.disconnect();
        deviceLine.classList.add("on");
        deviceLine.querySelectorAll(".spectrum-dot").forEach((dot, i) => {
          setTimeout(() => dot.classList.add("on"), 500 + i * 200);
        });
      },
      { threshold: 0.5 },
    );
    lineObs.observe(deviceLine);
  }

  // ═══ Quality Ring Animation ═══
  const qCards = document.querySelectorAll(".q-card");
  if (qCards.length) {
    const ringObs = new IntersectionObserver(
      (entries) => {
        entries.forEach((entry) => {
          if (entry.isIntersecting) {
            entry.target
              .querySelectorAll(".ring-fg")
              .forEach((r) => r.classList.add("on"));
            ringObs.unobserve(entry.target);
          }
        });
      },
      { threshold: 0.3 },
    );
    qCards.forEach((c) => ringObs.observe(c));
  }

  // ═══ Hero Canvas — lazy-loaded on desktop only ═══
  const heroCanvas = document.getElementById("hero-webgl") as HTMLCanvasElement;
  const isDesktop = window.matchMedia("(min-width: 769px)").matches;
  if (heroCanvas && isDesktop) {
    const heroObs = new IntersectionObserver(
      (entries) => {
        if (!entries[0]?.isIntersecting) return;
        heroObs.disconnect();
        const launch = () =>
          import("./hero-canvas").then((m) => m.initHeroCanvas(heroCanvas));
        const ric = (
          window as unknown as {
            requestIdleCallback?: (
              cb: () => void,
              opts?: { timeout: number },
            ) => number;
          }
        ).requestIdleCallback;
        if (ric) ric(launch, { timeout: 4000 });
        else setTimeout(launch, 2000);
      },
      { threshold: 0.01 },
    );
    heroObs.observe(heroCanvas);
  } else if (heroCanvas) {
    heroCanvas.style.display = "none";
  }
}
