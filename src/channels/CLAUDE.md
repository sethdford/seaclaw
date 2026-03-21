# src/channels/ — Messaging Channels

38 channel implementations, each connecting the agent to an external messaging platform. Every channel implements the `hu_channel_t` vtable.

## Vtable Contract

Every channel must implement `hu_channel_vtable_t`:

- `start(ctx)` — initialize connection (auth, polling, webhook)
- `stop(ctx)` — teardown connection
- `send(ctx, target, message, media)` — deliver a message
- `name(ctx)` — stable lowercase name (e.g., `"telegram"`, `"discord"`)
- `health_check(ctx)` — return true if channel is operational

Optional methods (may be NULL):

- `send_event` — rich event delivery
- `start_typing` / `stop_typing` — typing indicators
- `load_conversation_history` — retrieve past messages
- `get_response_constraints` — platform-specific limits (e.g., max chars)
- `react` — add reactions to messages
- `human_active_recently` — detect if the real user messaged this contact recently (daemon uses this to suppress assistant sends when the human is active)

## Channel capability matrix

Generated from designated initializers in each `src/channels/*.c` vtable. **history** = `load_conversation_history`, **human_active** = `human_active_recently`, **typing** = both `start_typing` and `stop_typing` non-NULL, **constraints** = `get_response_constraints`. Omitted optional fields are NULL.

Only **discord**, **imessage**, **signal**, **slack**, and **telegram** implement `human_active_recently`. On all other channels the daemon cannot suppress outbound messages based on recent human activity.

```c
/*
 * Channel capability matrix (vtable hooks):
 * Channel        | send | history | react | human_active | typing | constraints
 * cli            |  ✓   |    ·    |   ·   |      ·       |   ·    |     ·
 * dingtalk       |  ✓   |    ·    |   ·   |      ·       |   ·    |     ·
 * discord        |  ✓   |    ✓    |   ✓   |      ✓       |   ✓    |     ✓
 * dispatch       |  ✓   |    ·    |   ·   |      ·       |   ·    |     ·
 * email          |  ✓   |    ·    |   ·   |      ·       |   ·    |     ·
 * facebook       |  ✓   |    ·    |   ·   |      ·       |   ·    |     ·
 * gmail          |  ✓   |    ✓    |   ·   |      ·       |   ·    |     ·
 * google_chat    |  ✓   |    ·    |   ·   |      ·       |   ·    |     ·
 * google_rcs     |  ✓   |    ·    |   ·   |      ·       |   ·    |     ·
 * imap           |  ✓   |    ·    |   ·   |      ·       |   ·    |     ·
 * imessage       |  ✓   |    ✓    |   ✓   |      ✓       |   ·    |     ✓
 * instagram      |  ✓   |    ·    |   ·   |      ·       |   ·    |     ·
 * irc            |  ✓   |    ✓    |   ·   |      ·       |   ·    |     ·
 * lark           |  ✓   |    ·    |   ·   |      ·       |   ·    |     ·
 * line           |  ✓   |    ·    |   ·   |      ·       |   ·    |     ·
 * maixcam        |  ✓   |    ·    |   ·   |      ·       |   ·    |     ·
 * matrix         |  ✓   |    ✓    |   ·   |      ·       |   ·    |     ·
 * mattermost     |  ✓   |    ·    |   ·   |      ·       |   ·    |     ·
 * mqtt           |  ✓   |    ✓    |   ·   |      ·       |   ·    |     ·
 * nostr          |  ✓   |    ✓    |   ·   |      ·       |   ·    |     ·
 * onebot         |  ✓   |    ·    |   ·   |      ·       |   ·    |     ·
 * pwa            |  ✓   |    ·    |   ·   |      ·       |   ·    |     ·
 * qq             |  ✓   |    ·    |   ·   |      ·       |   ·    |     ·
 * signal         |  ✓   |    ✓    |   ✓   |      ✓       |   ✓    |     ✓
 * slack          |  ✓   |    ✓    |   ✓   |      ✓       |   ✓    |     ✓
 * teams          |  ✓   |    ·    |   ·   |      ·       |   ·    |     ·
 * telegram       |  ✓   |    ✓    |   ✓   |      ✓       |   ✓    |     ✓
 * tiktok         |  ✓   |    ·    |   ·   |      ·       |   ·    |     ·
 * twilio         |  ✓   |    ·    |   ·   |      ·       |   ·    |     ·
 * twitter        |  ✓   |    ·    |   ·   |      ·       |   ·    |     ·
 * voice_channel  |  ✓   |    ·    |   ·   |      ·       |   ·    |     ·
 * web            |  ✓   |    ·    |   ·   |      ·       |   ·    |     ·
 * whatsapp       |  ✓   |    ·    |   ✓   |      ·       |   ·    |     ✓
 *
 * Channels without human_active_recently: daemon cannot suppress messages
 * when the real user is active on those channels.
 */
```

| Channel | `load_conversation_history` | `react` | `human_active_recently` | typing | `get_response_constraints` |
| --- | --- | --- | --- | --- | --- |
| cli | · | · | · | · | · |
| dingtalk | · | · | · | · | · |
| discord | ✓ | ✓ | ✓ | ✓ | ✓ |
| dispatch | · | · | · | · | · |
| email | · | · | · | · | · |
| facebook | · | · | · | · | · |
| gmail | ✓ | · | · | · | · |
| google_chat | · | · | · | · | · |
| google_rcs | · | · | · | · | · |
| imap | · | · | · | · | · |
| imessage | ✓ | ✓ | ✓ | · | ✓ |
| instagram | · | · | · | · | · |
| irc | ✓ | · | · | · | · |
| lark | · | · | · | · | · |
| line | · | · | · | · | · |
| maixcam | · | · | · | · | · |
| matrix | ✓ | · | · | · | · |
| mattermost | · | · | · | · | · |
| mqtt | ✓ | · | · | · | · |
| nostr | ✓ | · | · | · | · |
| onebot | · | · | · | · | · |
| pwa | · | · | · | · | · |
| qq | · | · | · | · | · |
| signal | ✓ | ✓ | ✓ | ✓ | ✓ |
| slack | ✓ | ✓ | ✓ | ✓ | ✓ |
| teams | · | · | · | · | · |
| telegram | ✓ | ✓ | ✓ | ✓ | ✓ |
| tiktok | · | · | · | · | · |
| twilio | · | · | · | · | · |
| twitter | · | · | · | · | · |
| voice_channel | · | · | · | · | · |
| web | · | · | · | · | · |
| whatsapp | · | ✓ | · | · | ✓ |

## Channel Implementations

### Core / Direct

```
cli.c               Terminal-based interactive channel
imessage.c           Apple iMessage (macOS only)
web.c                Web-based chat via gateway
```

### Chat Platforms

```
discord.c            Discord bot (WebSocket + REST)
slack.c              Slack bot (Events API)
telegram.c           Telegram Bot API (polling)
teams.c              Microsoft Teams
mattermost.c         Mattermost webhooks
matrix.c             Matrix protocol
irc.c                IRC protocol
```

### Social / Messaging

```
whatsapp.c           WhatsApp Business API
signal.c             Signal messenger
line.c               LINE messaging
facebook.c           Facebook Messenger
instagram.c          Instagram Direct
twitter.c            Twitter/X DMs
tiktok.c             TikTok messaging
nostr.c              Nostr protocol
```

### Email

```
gmail.c              Gmail API
gmail_base64.c       Gmail base64 utilities
email.c              Generic SMTP/IMAP email
imap.c               IMAP polling
```

### Enterprise / Regional

```
google_chat.c        Google Chat (Workspace)
google_rcs.c         Google RCS Business Messaging
dingtalk.c           DingTalk (Alibaba)
lark.c               Lark/Feishu (ByteDance)
qq.c                 QQ messenger
onebot.c             OneBot protocol
```

### Voice & IoT

```
voice_channel.c      Voice call channel
voice_integration.c  Voice subsystem integration
voice_realtime.c     Real-time voice streaming
maixcam.c            MaixCAM IoT device
```

### PWA

```
pwa.c                Progressive Web App bridge channel
```

### Infrastructure

```
meta_common.c        Shared code for Meta platforms (Facebook, Instagram, WhatsApp)
twilio.c             Twilio SMS/voice
mqtt.c               MQTT protocol
dispatch.c           Message routing between channels
thread_binding.c     Thread-to-channel binding
```

## Policies

```c
hu_dm_policy_t       — ALLOW, DENY, or ALLOWLIST for direct messages
hu_group_policy_t    — OPEN, MENTION_ONLY, or ALLOWLIST for group messages
hu_channel_policy_t  — combines DM + group policies per channel
```

## Adding a New Channel

1. Create `src/channels/<name>.c` implementing `hu_channel_vtable_t`
2. Create `include/human/channels/<name>.h` if needed
3. Register in `src/channels/factory.c`
4. Add `HU_ENABLE_CHANNEL_<NAME>` feature flag in CMakeLists.txt if optional
5. Add tests for vtable wiring, auth/config, health check, send/receive
6. Use `HU_IS_TEST` guards for any network connections

## Rules

- `send` must handle platform-specific message limits (split long messages)
- `start` must validate credentials/config before connecting
- Never hardcode API keys — use config or secret store
- Use `HU_IS_TEST` for all network operations in tests
- Channel name must be stable and lowercase — it's used as a config key
