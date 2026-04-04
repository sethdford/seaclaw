/**
 * Hero canvas neural network animation.
 * Lazy-loaded on desktop only to keep the main bundle small.
 */

function createNoise2D(): (x: number, y: number) => number {
  const p = new Uint8Array(512);
  for (let i = 0; i < 256; i++) p[i] = i;
  let seed = 2463534242;
  for (let i = 255; i > 0; i--) {
    seed = (seed * 1664525 + 1013904223) >>> 0;
    const j = seed % (i + 1);
    const tmp = p[i];
    p[i] = p[j];
    p[j] = tmp;
  }
  for (let i = 0; i < 256; i++) p[i + 256] = p[i];
  const fade = (t: number) => t * t * t * (t * (t * 6 - 15) + 10);
  const lerp = (t: number, a: number, b: number) => a + t * (b - a);
  const grad2 = (hash: number, x: number, y: number) => {
    const h = hash & 3;
    return (h < 2 ? x : -x) + (h === 0 || h === 3 ? y : -y);
  };
  return (x: number, y: number) => {
    const X = Math.floor(x) & 255;
    const Y = Math.floor(y) & 255;
    const xf = x - Math.floor(x);
    const yf = y - Math.floor(y);
    const u = fade(xf);
    const v = fade(yf);
    const A = p[X] + Y;
    const B = p[X + 1] + Y;
    return lerp(
      v,
      lerp(u, grad2(p[A], xf, yf), grad2(p[B], xf - 1, yf)),
      lerp(
        u,
        grad2(p[A + 1], xf, yf - 1),
        grad2(p[B + 1], xf - 1, yf - 1),
      ),
    );
  };
}

export function initHeroCanvas(canvas: HTMLCanvasElement) {
  const ctx = canvas.getContext("2d");
  if (!ctx) {
    canvas.style.display = "none";
    return;
  }
  const section = canvas.closest("section")!;
  const CW = () => section.clientWidth;
  const CH = () => section.clientHeight;
  const dpr = Math.min(window.devicePixelRatio, 2);
  canvas.width = CW() * dpr;
  canvas.height = CH() * dpr;
  ctx.scale(dpr, dpr);

  const noise = createNoise2D();
  const accentHex =
    getComputedStyle(document.documentElement)
      .getPropertyValue("--hu-accent")
      .trim() || "#7AB648";
  const secondaryHex =
    getComputedStyle(document.documentElement)
      .getPropertyValue("--hu-accent-secondary")
      .trim() || "#f59e0b";
  function hexToRGB(hex: string): [number, number, number] {
    const v = parseInt(hex.replace("#", ""), 16);
    return [(v >> 16) & 255, (v >> 8) & 255, v & 255];
  }
  const accentRGB = hexToRGB(accentHex);
  const secondaryRGB = hexToRGB(secondaryHex);

  const layerSizes = [10, 12, 10, 12, 10];
  const layerX = [0.1, 0.28, 0.5, 0.72, 0.9];
  const nodeBase: [number, number][] = [];
  for (let L = 0; L < layerSizes.length; L++) {
    const n = layerSizes[L];
    for (let i = 0; i < n; i++) {
      const t = n > 1 ? i / (n - 1) : 0.5;
      nodeBase.push([layerX[L], 0.15 + t * 0.7]);
    }
  }
  const nodeCount = nodeBase.length;
  const orbitR = new Float32Array(nodeCount);
  const orbitP = new Float32Array(nodeCount);
  const breathP = new Float32Array(nodeCount);
  const isAccent = new Uint8Array(nodeCount);
  for (let i = 0; i < nodeCount; i++) {
    orbitR[i] = 3 + Math.random() * 6;
    orbitP[i] = Math.random() * Math.PI * 2;
    breathP[i] = Math.random() * Math.PI * 2;
    isAccent[i] = Math.random() < 0.2 ? 1 : 0;
  }
  const conns: [number, number][] = [];
  let lStart = 0;
  for (let L = 0; L < layerSizes.length - 1; L++) {
    const nCur = layerSizes[L],
      nNext = layerSizes[L + 1],
      nStart = lStart + nCur;
    for (let i = 0; i < nCur; i++) {
      const k = 2 + Math.floor(Math.random() * 3);
      const used = new Set<number>();
      for (let c = 0; c < Math.min(k, nNext); c++) {
        let t = Math.floor(Math.random() * nNext);
        while (used.has(t)) t = (t + 1) % nNext;
        used.add(t);
        conns.push([lStart + i, nStart + t]);
      }
    }
    lStart = nStart;
  }
  const FLOW_COUNT = 400;
  const flowX = new Float32Array(FLOW_COUNT);
  const flowY = new Float32Array(FLOW_COUNT);
  for (let i = 0; i < FLOW_COUNT; i++) {
    flowX[i] = Math.random();
    flowY[i] = Math.random();
  }

  let mx = -1,
    my = -1;
  const MOUSE_RAD = 120;
  const onMove = (e: MouseEvent) => {
    const r = canvas.getBoundingClientRect();
    mx = e.clientX - r.left;
    my = e.clientY - r.top;
  };
  const onLeave = () => {
    mx = -1;
    my = -1;
  };
  document.addEventListener("mousemove", onMove);
  document.addEventListener("mouseleave", onLeave);

  const FRAME_INTERVAL = 1000 / 60;
  let lastFrame = 0;
  let animId: number;
  const nodePos = new Float32Array(nodeCount * 2);

  const animate = (now: number) => {
    animId = requestAnimationFrame(animate);
    if (now - lastFrame < FRAME_INTERVAL) return;
    lastFrame = now;
    const cw = CW(),
      ch = CH();
    const t = now * 0.001;
    const breath = 1 + 0.04 * Math.sin(t * 0.8);
    ctx.clearRect(0, 0, cw, ch);

    for (let i = 0; i < nodeCount; i++) {
      const [bx, by] = nodeBase[i];
      let x =
        bx * cw + orbitR[i] * Math.cos(t * 0.6 + orbitP[i]) * breath;
      let y =
        by * ch +
        orbitR[i] * Math.sin(t * 0.5 + orbitP[i] * 1.1) * breath;
      x += 4 * noise(x * 0.005, t * 0.3);
      y += 4 * noise(y * 0.005 + 50, t * 0.3);
      if (mx >= 0) {
        const dx = x - mx,
          dy = y - my,
          d = Math.sqrt(dx * dx + dy * dy);
        if (d < MOUSE_RAD && d > 1) {
          const f = (1 - d / MOUSE_RAD) * 8;
          x += (dx / d) * f;
          y += (dy / d) * f;
        }
      }
      nodePos[i * 2] = x;
      nodePos[i * 2 + 1] = y;
    }

    for (let c = 0; c < conns.length; c++) {
      const [a, b] = conns[c];
      const ax = nodePos[a * 2],
        ay = nodePos[a * 2 + 1],
        bx = nodePos[b * 2],
        by = nodePos[b * 2 + 1];
      let alpha =
        0.08 * (0.6 + 0.4 * Math.sin((t * 3 + c * 0.1) * Math.PI * 2));
      if (mx >= 0) {
        const d = Math.sqrt(
          ((ax + bx) / 2 - mx) ** 2 + ((ay + by) / 2 - my) ** 2,
        );
        if (d < MOUSE_RAD * 1.5)
          alpha += 0.12 * (1 - d / (MOUSE_RAD * 1.5));
      }
      ctx.beginPath();
      ctx.moveTo(ax, ay);
      ctx.lineTo(bx, by);
      ctx.strokeStyle = `rgba(${accentRGB[0]},${accentRGB[1]},${accentRGB[2]},${alpha})`;
      ctx.lineWidth = 1;
      ctx.stroke();
    }

    for (let i = 0; i < nodeCount; i++) {
      const x = nodePos[i * 2],
        y = nodePos[i * 2 + 1];
      const rgb = isAccent[i] ? secondaryRGB : accentRGB;
      const a = Math.min(
        1,
        0.5 *
          (isAccent[i]
            ? 0.7 + 0.3 * Math.sin(t * 2.2 + breathP[i])
            : 1),
      );
      ctx.beginPath();
      ctx.arc(x, y, 2.5, 0, Math.PI * 2);
      ctx.fillStyle = `rgba(${rgb[0]},${rgb[1]},${rgb[2]},${a})`;
      ctx.fill();
    }

    const flowStep = 0.0008;
    for (let i = 0; i < FLOW_COUNT; i++) {
      flowX[i] += flowStep * noise(flowY[i] * 3, t * 0.3);
      flowY[i] += flowStep * noise(flowX[i] * 3 + 23.1, t * 0.3);
      if (flowX[i] > 1.1) flowX[i] -= 1.2;
      else if (flowX[i] < -0.1) flowX[i] += 1.2;
      if (flowY[i] > 1.1) flowY[i] -= 1.2;
      else if (flowY[i] < -0.1) flowY[i] += 1.2;
      ctx.beginPath();
      ctx.arc(flowX[i] * cw, flowY[i] * ch, 1, 0, Math.PI * 2);
      ctx.fillStyle = `rgba(${accentRGB[0]},${accentRGB[1]},${accentRGB[2]},0.12)`;
      ctx.fill();
    }

    if (mx >= 0) {
      const g = ctx.createRadialGradient(
        mx,
        my,
        0,
        mx,
        my,
        MOUSE_RAD * 0.8,
      );
      g.addColorStop(
        0,
        `rgba(${accentRGB[0]},${accentRGB[1]},${accentRGB[2]},0.08)`,
      );
      g.addColorStop(1, "transparent");
      ctx.fillStyle = g;
      ctx.fillRect(
        mx - MOUSE_RAD,
        my - MOUSE_RAD,
        MOUSE_RAD * 2,
        MOUSE_RAD * 2,
      );
    }
  };

  canvas.style.opacity = "1";
  canvas.style.transition = "opacity 1.5s ease-out";
  animate(performance.now());

  const onResize = () => {
    canvas.width = CW() * dpr;
    canvas.height = CH() * dpr;
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  };
  window.addEventListener("resize", onResize);
  const cleanObs = new IntersectionObserver(
    (entries) => {
      if (!entries[0]?.isIntersecting) cancelAnimationFrame(animId);
      else {
        lastFrame = 0;
        animate(performance.now());
      }
    },
    { threshold: 0 },
  );
  cleanObs.observe(canvas);
  const dispose = () => {
    cancelAnimationFrame(animId);
    cleanObs.disconnect();
    window.removeEventListener("resize", onResize);
    document.removeEventListener("mousemove", onMove);
    document.removeEventListener("mouseleave", onLeave);
  };
  window.addEventListener("pagehide", dispose, { once: true });
}
