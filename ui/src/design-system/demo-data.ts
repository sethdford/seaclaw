/**
 * Canonical demo fixtures for the in-app Design System view.
 * Tokens + components are the runtime source of truth; this file memorializes
 * representative datasets for charts, gauges, and flows (no gateway).
 */

import type { ChartData } from "../components/hu-chart.js";
import type { RingProgressItem } from "../components/hu-ring-progress.js";
import type { SankeyLink, SankeyNode } from "../components/hu-sankey.js";
import type { TimelineBar } from "../components/hu-timeline-chart.js";

/** Categorical ramp preview — aligns with `--hu-chart-categorical-*` */
export const DESIGN_CATEGORICAL_SWATCHES = [
  "var(--hu-chart-categorical-1)",
  "var(--hu-chart-categorical-2)",
  "var(--hu-chart-categorical-3)",
  "var(--hu-chart-categorical-4)",
  "var(--hu-chart-categorical-5)",
  "var(--hu-chart-categorical-6)",
] as const;

export const DEMO_BAR_CHART: ChartData = {
  labels: ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"],
  datasets: [
    {
      label: "Requests",
      data: [120, 190, 150, 220, 180, 240, 210],
    },
    {
      label: "Errors",
      data: [4, 2, 6, 1, 3, 0, 2],
    },
  ],
};

export const DEMO_LINE_CHART: ChartData = {
  labels: ["W1", "W2", "W3", "W4", "W5", "W6", "W7", "W8"],
  datasets: [
    {
      label: "Latency p50 (ms)",
      data: [42, 38, 44, 36, 33, 35, 31, 29],
    },
  ],
};

export const DEMO_DOUGHNUT_CHART: ChartData = {
  labels: ["Chat", "Tools", "Batch", "Voice"],
  datasets: [
    {
      data: [48, 22, 18, 12],
    },
  ],
};

/** 12 weeks × 7 days, 0–4 activity level */
export function demoHeatmapData(): number[] {
  const out: number[] = [];
  let seed = 42;
  for (let i = 0; i < 12 * 7; i++) {
    seed = (seed * 1103515245 + 12345) & 0x7fffffff;
    out.push(seed % 5);
  }
  return out;
}

export const DEMO_RING_PROGRESS: RingProgressItem[] = [
  { value: 0.78, max: 1, label: "Tokens", color: "var(--hu-chart-categorical-1)" },
  { value: 0.52, max: 1, label: "Budget", color: "var(--hu-chart-categorical-2)" },
  { value: 1.08, max: 1, label: "Sessions", color: "var(--hu-chart-categorical-3)" },
];

export const DEMO_SANKEY_NODES: SankeyNode[] = [
  { id: "prompt", label: "Prompt", column: 0 },
  { id: "router", label: "Router", column: 1 },
  { id: "model", label: "Model", column: 2 },
  { id: "tools", label: "Tools", column: 2 },
  { id: "response", label: "Response", column: 3 },
];

export const DEMO_SANKEY_LINKS: SankeyLink[] = [
  { from: "prompt", to: "router", value: 100 },
  { from: "router", to: "model", value: 72 },
  { from: "router", to: "tools", value: 28 },
  { from: "model", to: "response", value: 72 },
  { from: "tools", to: "response", value: 28 },
];

export const DEMO_TIMELINE_BARS: TimelineBar[] = [
  {
    id: "a",
    label: "Context compile",
    start: "2025-03-01",
    end: "2025-03-08",
    status: "complete",
  },
  {
    id: "b",
    label: "Provider rollout",
    start: "2025-03-06",
    end: "2025-03-22",
    status: "active",
  },
  {
    id: "c",
    label: "Eval harness",
    start: "2025-03-18",
    end: "2025-04-05",
    status: "planned",
  },
];

/** ISO date for “today” line in timeline demo */
export const DEMO_TIMELINE_TODAY = "2025-03-20";

export const DEMO_FORECAST_HISTORY = [
  { date: "2025-03-01", cost: 1.2 },
  { date: "2025-03-02", cost: 2.1 },
  { date: "2025-03-03", cost: 1.8 },
  { date: "2025-03-04", cost: 2.4 },
  { date: "2025-03-05", cost: 2.9 },
  { date: "2025-03-06", cost: 3.2 },
  { date: "2025-03-07", cost: 3.0 },
  { date: "2025-03-08", cost: 3.5 },
  { date: "2025-03-09", cost: 3.8 },
  { date: "2025-03-10", cost: 4.1 },
];

export const DEMO_SPARKLINE = [12, 14, 11, 18, 22, 19, 24, 28, 26, 30];
