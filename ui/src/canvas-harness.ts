/**
 * Runtime harness template for sandboxed canvas iframe.
 *
 * This HTML document is injected via iframe srcdoc. It renders agent content
 * (HTML, SVG, React/JSX, Mermaid, Markdown, code) inside a sandbox with no
 * access to the parent origin. Communication happens via postMessage only.
 *
 * Security: sandbox="allow-scripts" — no allow-same-origin, no allow-popups,
 * no allow-top-navigation.
 */

export function buildHarness(): string {
  return `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta http-equiv="Content-Security-Policy"
  content="default-src 'none'; script-src 'unsafe-inline' 'unsafe-eval' https://esm.sh; style-src 'unsafe-inline' https://esm.sh; img-src * data: blob:; font-src https://esm.sh; connect-src https://esm.sh;">
<style>
  *, *::before, *::after { box-sizing: border-box; }
  html, body {
    margin: 0; padding: 0;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
    font-size: 14px; line-height: 1.5;
    color: #e0e0e0; background: transparent;
    overflow: hidden;
  }
  #root { padding: 8px; }
  #root :where(img, svg) { max-width: 100%; height: auto; }
  .harness-error {
    color: #ff6b6b; background: #2d1515; border: 1px solid #5a2020;
    border-radius: 6px; padding: 12px; margin: 8px; font-family: monospace;
    font-size: 12px; white-space: pre-wrap;
  }
  pre { background: #1a1a2e; border-radius: 6px; padding: 12px; overflow-x: auto; }
  code { font-family: "SF Mono", "Fira Code", monospace; font-size: 13px; }
</style>
</head>
<body>
<div id="root"></div>
<script>
(function() {
  "use strict";
  var root = document.getElementById("root");
  var currentFormat = "html";
  var cdnCache = {};
  var resizeObserver = null;

  function postUp(msg) {
    try { window.parent.postMessage(msg, "*"); } catch(e) {}
  }

  function showError(err) {
    root.innerHTML = '<div class="harness-error">' + escapeHtml(String(err)) + '</div>';
    postUp({ type: "canvas:error", error: String(err) });
  }

  function escapeHtml(s) {
    return s.replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/"/g,"&quot;");
  }

  function notifyResize() {
    var h = document.documentElement.scrollHeight || 0;
    postUp({ type: "canvas:resize", height: h });
  }

  function setupResizeObserver() {
    if (resizeObserver) resizeObserver.disconnect();
    if (typeof ResizeObserver !== "undefined") {
      resizeObserver = new ResizeObserver(function() { notifyResize(); });
      resizeObserver.observe(root);
    }
  }

  function loadScript(url) {
    if (cdnCache[url]) return cdnCache[url];
    cdnCache[url] = new Promise(function(resolve, reject) {
      var s = document.createElement("script");
      s.type = "module";
      s.src = url;
      s.onload = resolve;
      s.onerror = function() { reject(new Error("Failed to load: " + url)); };
      document.head.appendChild(s);
    });
    return cdnCache[url];
  }

  async function loadESM(url) {
    try { return await import(url); } catch(e) { throw new Error("ESM import failed: " + url); }
  }

  async function renderReact(code, imports) {
    try {
      if (imports && Object.keys(imports).length > 0) {
        var existing = document.querySelector('script[type="importmap"]');
        if (existing) existing.remove();
        var im = document.createElement("script");
        im.type = "importmap";
        im.textContent = JSON.stringify({ imports: imports });
        document.head.appendChild(im);
      }

      var React = await loadESM("https://esm.sh/react@19");
      var ReactDOM = await loadESM("https://esm.sh/react-dom@19/client");
      var Babel = await loadESM("https://esm.sh/@babel/standalone");

      var transformed = Babel.transform(code, {
        presets: ["react"],
        filename: "canvas.jsx"
      }).code;

      var module = { exports: {} };
      var fn = new Function("React", "module", "exports", "require",
        transformed + "\\nif (typeof App !== 'undefined') module.exports = App;" +
        "\\nif (typeof default_1 !== 'undefined') module.exports = default_1;");
      fn(React, module, module.exports, function(name) {
        if (name === "react") return React;
        if (name === "react-dom/client") return ReactDOM;
        throw new Error("Cannot require: " + name);
      });

      var Component = module.exports.default || module.exports;
      if (typeof Component === "function") {
        root.innerHTML = "";
        var reactRoot = ReactDOM.createRoot(root);
        reactRoot.render(React.createElement(Component));
      } else {
        root.innerHTML = transformed;
      }
    } catch(e) {
      showError("React render error: " + e.message);
    }
  }

  async function renderMermaid(code) {
    try {
      var mermaid = await loadESM("https://esm.sh/mermaid@11");
      mermaid.default.initialize({ startOnLoad: false, theme: "dark" });
      var id = "mermaid-" + Date.now();
      var result = await mermaid.default.render(id, code);
      root.innerHTML = result.svg;
    } catch(e) {
      showError("Mermaid render error: " + e.message);
    }
  }

  function renderMarkdown(code) {
    try {
      var html = code
        .replace(/^### (.+)$/gm, "<h3>$1</h3>")
        .replace(/^## (.+)$/gm, "<h2>$1</h2>")
        .replace(/^# (.+)$/gm, "<h1>$1</h1>")
        .replace(/\\*\\*(.+?)\\*\\*/g, "<strong>$1</strong>")
        .replace(/\\*(.+?)\\*/g, "<em>$1</em>")
        .replace(/\`\`\`([\\s\\S]*?)\`\`\`/g, "<pre><code>$1</code></pre>")
        .replace(/\`(.+?)\`/g, "<code>$1</code>")
        .replace(/\\n/g, "<br>");
      root.innerHTML = html;
    } catch(e) {
      showError("Markdown render error: " + e.message);
    }
  }

  function renderCode(code, language) {
    root.innerHTML = '<pre><code class="language-' + escapeHtml(language || "text") + '">'
      + escapeHtml(code) + '</code></pre>';
  }

  async function renderContent(data) {
    var content = data.content || "";
    var format = data.format || "html";
    var imports = data.imports || {};
    var language = data.language || "";
    currentFormat = format;

    if (!content.trim()) {
      root.innerHTML = '<div style="text-align:center;padding:2rem;opacity:0.5">Empty canvas</div>';
      notifyResize();
      return;
    }

    try {
      switch(format) {
        case "react":
          await renderReact(content, imports);
          break;
        case "mermaid":
          await renderMermaid(content);
          break;
        case "markdown":
          renderMarkdown(content);
          break;
        case "code":
          renderCode(content, language);
          break;
        case "svg":
        case "html":
        case "mockup":
        default:
          root.innerHTML = content;
          break;
      }
    } catch(e) {
      showError("Render error: " + e.message);
    }

    notifyResize();
  }

  window.addEventListener("message", function(e) {
    var d = e.data;
    if (!d || typeof d !== "object") return;
    if (d.type === "canvas:render") {
      renderContent(d);
    }
  });

  setupResizeObserver();
  postUp({ type: "canvas:ready" });
})();
</script>
</body>
</html>`;
}
