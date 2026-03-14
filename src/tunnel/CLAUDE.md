# src/tunnel/ — Network Tunneling

Tunnel backends for exposing local server to the internet. Implements `hu_tunnel_t` vtable. Supports ngrok, Cloudflare, Tailscale, and custom tunnels.

## Key Files

- `ngrok.c` — ngrok CLI tunnel (`ngrok http <port>`)
- `cloudflare.c` — Cloudflare tunnel
- `tailscale.c` — Tailscale funnel
- `custom.c` — custom tunnel config
- `root.c` — tunnel factory/registration
- `none.c` — no-op implementation

## Rules

- `HU_IS_TEST`: returns mock URL, no process spawn
- Use `hu_process_run` / `popen` for CLI tunnels
- Free `public_url` on stop; caller owns returned URL
