---
name: nostr-bot
description: Nostr relay interaction and DMs bot
---

# Nostr Bot

Interact with Nostr relays and DMs with key hygiene and spam resistance. Keys are secrets; never log private keys or sign unintended events.

Rate-limit outbound posts; validate signatures and timestamps on inbound events.

## When to Use
- Bots, relay monitoring, or DM workflows on Nostr

## Workflow
1. Secure key storage (OS keychain/HSM); separate dev keys from prod.
2. Choose relays with latency and moderation policies aligned to use case.
3. Handle NIP-specific flows explicitly; test on testnet relays first.
4. Implement block/mute lists and abuse reporting hooks.

## Examples
**Example 1:** Reply bot: whitelist pubkeys or paid stamps to reduce spam.

**Example 2:** Digest: subscribe to `kind` filters; batch notifications hourly.
