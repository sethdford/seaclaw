---
title: "iMessage Capability Matrix"
description: "Canonical reference for iMessage capabilities, implementation status, feasibility, and priority."
---

# iMessage Capability Matrix

## Overview

Canonical reference for every iMessage platform capability, our implementation status, user behavior data, technical feasibility, and priority. Cross-references existing investigation docs and third-party tools.

## Platform Capabilities

| Feature | User Behavior | Feasibility | Our Status | Priority | Notes |
| --- | --- | --- | --- | --- | --- |
| **Send text** | Core (100%) | Public API (AppleScript or imsg CLI) | Implemented | — | `imessage_send()` via osascript; opt-in `imsg send` path via `HU_IMESSAGE_SEND_IMSG` |
| **Receive/poll text** | Core (100%) | chat.db SQLite | Implemented | — | `hu_imessage_poll()` via chat.db |
| **Send attachment** | High (~40% of convos) | AppleScript attachment | Implemented | — | osascript + file path |
| **Receive attachment path** | High | chat.db + attachment table | Implemented | — | `hu_imessage_get_attachment_path()` |
| **Classic tapback (send)** | Very high (70%+ of users) | JXA + AX automation | Implemented (fragile) | High | Requires AX permission, varies by macOS version. `HU_IMESSAGE_TAPBACK_ENABLED` opt-in. |
| **Classic tapback (read)** | Very high | chat.db types 2000-2005 | Implemented | — | `hu_imessage_build_tapback_context()` |
| **Custom emoji reaction (read)** | Growing (iOS 17+) | chat.db type 2006 | **Implemented** (this overhaul) | — | Added in this overhaul, aggregated in tapback context |
| **Custom emoji reaction (send)** | Growing | imsg CLI or private API | Not implemented | Medium | imsg v0.5.0 `react` command supports custom emoji |
| **Read receipts (read)** | ~77% of users enable | chat.db date_read column | Implemented | — | `hu_imessage_build_read_receipt_context()` |
| **Read receipts (send)** | — | No public API | Not possible | — | Apple controls this per-contact |
| **Typing indicator (send)** | High (humanizing) | Broken on macOS 26 | Not implemented | Low | imsg had typing but broken on Tahoe (issue #60, imagent requires private entitlements) |
| **Typing indicator (read)** | — | No public API | Not possible | — | Ephemeral, not stored in chat.db |
| **Inline reply (read)** | Moderate | chat.db thread_originator_guid | Implemented | — | `hu_imessage_lookup_message_by_guid()` |
| **Inline reply (send)** | Moderate | No public API | Not possible | — | Neither AppleScript nor imsg support this |
| **Edit message (send)** | 1 in 50 messages | Not feasible (see investigation) | Not implemented | — | See `imessage-edit-feasibility.md`. IMCore private API only. |
| **Unsend message** | 1 in 50 messages | Partially feasible (AX automation) | Not implemented | Low | See `imessage-unsend-feasibility.md`. 2-minute window, fragile. |
| **Abandoned typing** | Humanizing pattern | Not feasible (see investigation) | Not implemented | — | See `imessage-abandoned-typing-feasibility.md` |
| **GIF send** | Common | Tenor API + AppleScript attachment | Implemented | — | `hu_imessage_fetch_gif()` + send as attachment |
| **GIF tapback tracking** | — | chat.db | Implemented | — | `hu_imessage_count_recent_gif_tapbacks()` (now includes type 2006) |
| **Sticker/Memoji (read)** | 1B+ Memoji weekly, 40% created | chat.db balloon_bundle_id | Not implemented | Medium | Stickers have `balloon_bundle_id` like `com.apple.Stickers.*` |
| **Sticker/Memoji (send)** | — | No public API | Not possible | — | No AppleScript/CLI path |
| **Group chat (send)** | High | AppleScript (via chat name) | Implemented | — | Works with group chat identifiers |
| **Group chat (poll)** | High | chat.db | Implemented | — | Polls from all chats |
| **Contact allow-list** | — | Internal | Implemented | — | `allow_from` filter in create() |
| **Health check** | — | chat.db accessibility | Implemented | — | Checks if chat.db is readable |
| **attributedBody parsing** | Required for macOS 15+ | Binary blob parsing | **Implemented** (this overhaul) | — | Now shared across channels, feeds, persona sampler |
| **User activity detection** | — | chat.db is_from_me | Implemented | — | `hu_imessage_user_responded_recently()` |
| **Unsent message detection** | — | chat.db | Implemented | — | `was_unsent` field in poll output |

## Third-Party Tool Comparison

| Tool | Tapback Send | Typing | Edit | Unsend | Inline Reply | Speed | API Type |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **Our implementation** | JXA+AX (fragile) | No | No | No | No | ~2-5s send | AppleScript |
| **imsg v0.5.0** | Yes (`react` cmd) | Broken (macOS 26) | No | No | No | <1s send | AppleScript + Swift |
| **imsg-bridge** | Via imsg | Broken | No | No | No | ~1.2s avg | REST wrapper around imsg |
| **BlueBubbles** | Yes | Yes | Yes (private API) | Yes (private API) | Yes | ~2s | Private IMCore |
| **iMessage-Kit** | No | No | No | No | No | — | AppleScript wrapper |

## User Behavior Data (2025-2026)

- Tapback usage: 70%+ of iMessage users use tapbacks regularly
- Unsend usage: ~1 in 50 messages (higher than previously estimated)
- Edit usage: ~1 in 50 messages
- Sticker/Memoji usage: 1B+ Memoji stickers sent weekly; 40% of users have created a Memoji
- Read receipts: ~77% of users have them enabled
- GIF usage: Common in casual conversations, tracked via Tenor API integration
- Sources: worldmetrics.org 2026 iMessage statistics, Apple WWDC presentations

## Existing Investigation Cross-References

| Investigation | File | Conclusion | Changed Since? |
| --- | --- | --- | --- |
| Edit messages | `imessage-edit-feasibility.md` (2026-03-08) | Not feasible via public APIs | No — imsg still has no edit |
| Unsend messages | `imessage-unsend-feasibility.md` (2026-03-08) | Partially feasible (AX automation) | No — imsg has no unsend |
| Abandoned typing | `imessage-abandoned-typing-feasibility.md` (2026-03-10) | Not feasible | Minor — imsg v0.5.0 had typing but broken on macOS 26 |

## Decision Log

| Decision | Rationale | Date |
| --- | --- | --- |
| Use AppleScript for send | Public API, no private deps, works across macOS versions | Original |
| JXA+AX for tapback send | Only viable public path for reactions | Original |
| Opt-in tapback via CMake flag | Fragile, requires AX permission | Original |
| Add custom emoji reaction read (type 2006) | iOS 17+ feature, read-only via chat.db, zero risk | 2026-04 |
| Extract attributedBody parser to shared | Needed by feeds + persona sampler, pure byte parsing | 2026-04 |
| Evaluate imsg CLI for tapback | Potentially more reliable than JXA, active maintenance | 2026-04 (pending) |
| Do not implement edit/unsend | Private API only, high maintenance risk | 2026-03 |
| Do not implement typing | Broken on macOS 26 even via imsg | 2026-04 |

## Architecture

- **Outbound**: `imessage_send()` → osascript AppleScript → Messages.app
- **Inbound**: `hu_imessage_poll()` → chat.db SQLite → parse text + attributedBody + attachments + reactions
- **Tapback send**: `imessage_react()` → JXA + System Events AX automation (opt-in)
- **Feed/persona**: `feeds/imessage.c` and `persona/sampler.c` → chat.db with attributedBody support
- **GIF**: Tenor API → download to temp file → send as attachment

## Last Updated

2026-04-03
