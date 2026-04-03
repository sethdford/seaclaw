# Canvas Security Model

## Overview

The Live Canvas renders agent-generated content (HTML, SVG, React/JSX, Mermaid, Markdown, code) in a sandboxed iframe. This document defines the security boundaries, threat model, and mitigations.

## Sandbox Architecture

Content renders inside an `<iframe sandbox="allow-scripts" srcdoc="...">`:

- **`allow-scripts`**: Required for React/Mermaid rendering and dynamic content.
- **No `allow-same-origin`**: The iframe cannot access the parent dashboard's cookies, localStorage, sessionStorage, or WebSocket connection.
- **No `allow-top-navigation`**: The iframe cannot navigate the parent frame.
- **No `allow-popups`**: The iframe cannot open new windows.
- **No `allow-forms`**: The iframe cannot submit forms.

## Content Security Policy

The harness HTML includes a restrictive CSP meta tag:

```
default-src 'none';
script-src 'unsafe-inline' 'unsafe-eval' https://esm.sh;
style-src 'unsafe-inline' https://esm.sh;
img-src * data: blob:;
font-src https://esm.sh;
connect-src https://esm.sh;
```

- Scripts: Only inline scripts and `esm.sh` CDN are allowed. This is necessary for Babel transpilation (`unsafe-eval`) and React rendering.
- Styles: Inline styles and `esm.sh` CSS modules.
- Images: Permissive (agent content may reference external images).
- Network: Only `esm.sh` for CDN package loading. No arbitrary fetch/XHR.

## Communication Protocol

Parent-to-iframe and iframe-to-parent communication uses `window.postMessage()`:

| Direction | Message Type | Purpose |
|-----------|-------------|---------|
| iframe → parent | `canvas:ready` | Harness initialized, ready for content |
| iframe → parent | `canvas:error` | Rendering error occurred |
| iframe → parent | `canvas:resize` | Content height changed (for auto-sizing) |
| parent → iframe | `canvas:render` | New content to render (includes format, content, imports) |

The parent validates `e.source === iframe.contentWindow` before processing messages.

## Threat Model

| Threat | Mitigation |
|--------|-----------|
| XSS from agent content | Sandboxed iframe without `allow-same-origin` — no DOM access to parent |
| Cookie/localStorage theft | No `allow-same-origin` — iframe has separate storage partition |
| Data exfiltration via fetch | CSP `connect-src` restricts to `esm.sh` only |
| Popup/redirect phishing | No `allow-popups`, no `allow-top-navigation` |
| Script injection into parent | `postMessage` validated by source check; parent only processes known message types |
| CDN supply chain attack | Only `esm.sh` allowed; pinned major versions (React 19, Babel) |

## Allowed CDN Domains

- `https://esm.sh` — JavaScript module CDN (React, Babel, Mermaid, user-specified imports)

No other external domains are allowed for script or style loading.

## User Edit Security

User edits via CodeMirror are debounced and sent to the gateway via `canvas.edit` RPC. The edit content is stored server-side on the canvas entry and flagged as `user_edit_pending`. The agent receives the edit in its next turn context. User edits do not bypass the sandbox — they flow through the same postMessage rendering pipeline.

## Offline/Air-gapped Behavior

When CDN access is unavailable:
- `html`, `svg`, `mockup`, `markdown`, `code` formats render normally (no CDN dependency).
- `react` format: Babel/React CDN load fails; harness shows error banner with raw JSX source as fallback.
- `mermaid` format: Mermaid CDN load fails; harness shows raw Mermaid syntax.

## Version History

Version history is stored in-memory in the C backend (32-entry ring buffer per canvas). No persistent storage — versions are lost on process restart. This is acceptable for the current use case (ephemeral agent sessions).
