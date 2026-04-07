// Defer overview preload to idle — default tab is chat, so this shouldn't
// compete with the critical path for initial render / TTI / LCP.
if ("requestIdleCallback" in globalThis) {
  requestIdleCallback(() => void import("./views/overview-view.js"));
} else {
  setTimeout(() => void import("./views/overview-view.js"), 2000);
}
