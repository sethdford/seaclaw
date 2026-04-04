# E2E iMessage Agent-to-Agent Test

Two `human` instances talking to each other over real iMessage, proving the full pipeline works end-to-end: poll → classify → agent turn → send → repeat.

## Prerequisites

- **Two Macs** (or one Mac + one Mac VM), each with its own Apple ID
- **Full Disk Access** granted to the terminal app on both machines (for `chat.db` read access)
- `human` built and configured on both machines
- Both Apple IDs must have iMessage enabled
- Optional: `brew install steipete/tap/imsg` for faster sends via imsg CLI

## Quick Start

### Mac A (initiator)

```bash
# One command does everything: sends seed, starts listening, auto-stops after 5 turns
human channel e2e-test --target +1BBBBBBBBB --turns 5
```

### Mac B (responder)

```bash
# Run the service loop with E2E-friendly config (see Config section below)
human service-loop
```

Mac A sends the seed message, Mac B picks it up and responds, Mac A picks that up and responds, and so on — back and forth over real iMessage until the turn limit is reached.

## CLI Reference

```
human channel e2e-test [options]

Options:
  --target <phone/email>   iMessage handle of the other agent (defaults to
                           channels.imessage.default_target from config)
  --seed <message>         Opening message to kick off the conversation
                           (default: "Hey! Just testing our connection —
                           how's everything going?")
  --turns <N>              Stop after N agent responses (default: 5)
```

The command automatically overrides daemon settings for E2E mode:
- `max_consecutive_replies` → 0 (unlimited)
- `e2e_max_turns` → N (from `--turns`)
- `response_mode` → "eager" (respond to everything)

## Config

### Mac A (initiator)

Mac A uses `human channel e2e-test` which handles config overrides automatically. Minimal config needed:

```json
{
  "channels": {
    "imessage": {
      "default_target": "+1BBBBBBBBB",
      "allow_from": ["+1BBBBBBBBB"],
      "use_imsg_cli": true
    }
  }
}
```

### Mac B (responder)

Mac B runs `human service-loop`, so it needs explicit daemon overrides in `~/.human/config.json`:

```json
{
  "channels": {
    "imessage": {
      "default_target": "+1AAAAAAAAA",
      "allow_from": ["+1AAAAAAAAA"],
      "use_imsg_cli": true,
      "daemon": {
        "max_consecutive_replies": 0,
        "e2e_max_turns": 5,
        "response_mode": "eager"
      }
    }
  }
}
```

### Config Fields

| Field | Where | Default | Description |
|-------|-------|---------|-------------|
| `daemon.max_consecutive_replies` | Any channel's daemon block | 3 | Max agent replies before going silent. `0` = unlimited. |
| `daemon.e2e_max_turns` | Any channel's daemon block | 0 | Auto-stop daemon after N total agent responses. `0` = no limit. |
| `daemon.response_mode` | Any channel's daemon block | `"selective"` | `"eager"` responds to everything, recommended for E2E. |

These fields work on **any channel**, not just iMessage — you could run the same test over Telegram, Discord, etc.

## What Gets Tested

A successful run proves every layer of the stack works in production conditions:

1. **Inbound poll** — `chat.db` SQLite queries, `last_rowid` tracking, `allow_from` filtering
2. **Message batching** — consecutive messages from the same sender merged into one prompt
3. **Response classifier** — eager mode overrides selective gating
4. **Agent turn** — full LLM round-trip (provider, tools, context, history)
5. **Outbound send** — `imsg` CLI or AppleScript delivery
6. **Echo suppression** — sent-message ring prevents processing our own outbound as inbound
7. **Turn limiting** — daemon auto-stops after the configured number of responses

## Troubleshooting

### Agent goes silent after 3 replies

The consecutive reply limiter is active. Ensure `daemon.max_consecutive_replies` is set to `0` in the responder's config, or use `human channel e2e-test` on the initiator side (it overrides automatically).

### Messages not being picked up

- Check `allow_from` includes the other agent's handle (exact match on phone number or email)
- Verify Full Disk Access is granted for the terminal app
- Check poll interval: default is 30s, which means up to 30s latency per turn
- Run `human doctor` to verify iMessage health

### Send failures

- If using `imsg` CLI: verify `which imsg` returns a path
- If using AppleScript: Messages.app must be running
- Check the console for `[e2e]` prefixed log lines

### Persona blocking

If you have a persona configured, the other agent's handle must appear as a contact in the persona file. Unknown senders are dropped when persona is active.

## Architecture Notes

The E2E test reuses the production daemon loop unchanged — there is no special "test mode" bypass. The only differences from normal operation are:

- **Consecutive reply limiter disabled** (`max_consecutive_replies: 0`) — normally caps at 3 to prevent runaway loops with real humans
- **Turn budget enforced** (`e2e_max_turns: N`) — ensures the test terminates
- **Eager response mode** — responds to everything instead of selectively gating

This means a successful E2E run is a genuine proof that the production pipeline works.
