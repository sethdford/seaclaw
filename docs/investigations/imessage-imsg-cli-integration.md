---
title: "iMessage imsg CLI Integration Feasibility"
description: "Investigation into integrating the steipete/imsg CLI for more stable iMessage tapback sending."
---

# iMessage imsg CLI Integration Feasibility

**Date:** 2026-04-03  
**Status:** Investigation  
**Related:** `imessage-capability-matrix.md`, `imessage-edit-feasibility.md`

## Background

Our current tapback (reaction) sending uses JXA + System Events accessibility automation, which is fragile, requires Accessibility permissions, and varies by macOS version. The `steipete/imsg` CLI v0.5.0 (released 2026-02-16, 950+ GitHub stars, Swift) now supports `imsg react` via a more stable approach.

## imsg v0.5.0 Capabilities

| Feature | Command | Status | Notes |
|---------|---------|--------|-------|
| Send text | `imsg send` | Working | Uses AppleScript internally, <1s latency |
| Send attachment | `imsg send --attachment` | Working | File path attachment |
| List chats | `imsg chats` | Working | JSON output for tooling |
| Message history | `imsg history` | Working | Read-only via chat.db |
| Watch new messages | `imsg watch` | Working | Filesystem event-driven monitoring |
| Send tapback | `imsg react` | Working | Supports love, like, dislike, laugh, emphasis, question, and custom emoji |
| Typing indicators | `imsg typing` | **Broken** on macOS 26 | imagent requires Apple-private entitlements (issue #60) |
| Phone normalization | built-in | Working | E.164 format with configurable region |

## Evaluation Criteria

### 1. Runtime Requirement
imsg is a Swift binary distributed via Homebrew (`brew install steipete/tap/imsg`) or direct download. Options:
- **Expect user to install**: Simplest. Document as optional dependency. Detect at runtime via `which imsg`.
- **Bundle with Human**: Binary is ~2-3MB. Increases our footprint. Licensing: MIT.
- **Homebrew auto-install**: Could auto-install on first use if Homebrew available.

**Recommendation:** Detect on `$PATH` at runtime. Document installation in setup guide. Do not bundle.

### 2. API Surface for Tapbacks
We have the data needed for `imsg react`:
- Chat GUID: available from poll (chat.db `chat.guid`)
- Message GUID: available from poll (chat.db `message.guid`)
- Reaction type: maps directly to imsg's supported types

Command: `imsg react <chat-guid> <message-guid> <reaction-type>`

This is more reliable than our JXA+AX approach because:
- No Accessibility permission needed
- No UI element positioning required
- No System Events fragility
- Works headless (no Messages.app window needed)

### 3. Performance
| Method | Send Latency | Tapback Latency |
|--------|-------------|-----------------|
| Our AppleScript | ~2-5s | N/A (JXA path) |
| Our JXA tapback | Variable, often fails | ~3-10s when it works |
| imsg send | <1s | N/A |
| imsg react | <1s (estimated) | <1s |
| imsg-bridge (REST) | ~1.2s avg | Via imsg |

### 4. Typing Indicators
**Status: Broken on macOS 26 (Tahoe).** The `imagent` daemon now enforces Apple-private entitlements for the XPC service `com.apple.imagent.desktop.auth`. Third-party binaries cannot access it. See issue #60. Also, `any;-;` GUID prefixes are not recognized by IMCore (issues #54, #56).

**Conclusion:** Do not implement typing indicators via imsg. This is a dead end until Apple changes the entitlement policy.

### 5. Inline Replies
Not supported by imsg. Confirmed dead end — same as our finding.

### 6. Risk Assessment
| Risk | Severity | Mitigation |
|------|----------|------------|
| imsg project goes dormant | Medium | MIT license, fork possible; graceful fallback to JXA |
| macOS update breaks imsg | Medium | imsg uses AppleScript for send (stable); react path is newer |
| User doesn't have imsg installed | Low | Graceful fallback: detect on $PATH, fall back to JXA or return HU_ERR_NOT_SUPPORTED |
| Binary compatibility | Low | imsg targets macOS 14+; we target macOS 13+ |

## Recommendation

**Implement imsg-based tapback as an alternative path.**

### Implementation Plan
1. Add `HU_IMESSAGE_TAPBACK_IMSG` CMake option (parallel to existing `HU_IMESSAGE_TAPBACK_ENABLED`)
2. In `imessage_react()`, add detection: check if `imsg` exists on `$PATH` at channel init
3. If available, invoke `imsg react <chat-guid> <message-guid> <reaction>` via `hu_process_run`
4. Graceful fallback chain: imsg → JXA (if enabled) → HU_ERR_NOT_SUPPORTED
5. Store chat GUID and message GUID in poll output (already available from chat.db)

### Send Path Evaluation
`imsg send` also offers advantages:
- Faster (<1s vs ~2-5s for raw AppleScript)
- Better error handling (JSON output)
- Attachment support via `--file` flag
- Named argument syntax: `imsg send --to <recipient> --text "msg" [--file /path] --service imessage`

Our current AppleScript send path is stable and well-tested. The imsg send path was initially an opt-in alternative via a CMake flag; it has since been promoted to the preferred path controlled by the `use_imsg_cli` runtime config key (the CMake flag was removed 2026-04-06).

**Implementation (2026-04-03):** `use_imsg_cli` config key added. When enabled:
- Text sends try `imsg send --to <target> --text <message> --service imessage` first
- Attachment sends use `imsg send --to <target> --file <path> --service imessage`
- Graceful fallback: if imsg fails, falls through to existing AppleScript path
- Media-only sends (no text) also support imsg with AppleScript fallback

**Update (2026-04-04):** Both the send and react paths are now implemented in C:

- Text send via `imsg send --to <target> --text <message> --service imessage` with AppleScript fallback
- imsg CLI tapback works even without `HU_IMESSAGE_TAPBACK_ENABLED` — auto-detects `imsg` on `$PATH` and tries `imsg react --chat-id <chat-guid> --message-guid <msg-guid> --reaction <type>` before returning `HU_ERR_NOT_SUPPORTED`
- Chat GUID and message GUID are looked up from chat.db at react time
- `use_imsg_cli` config key now parsed in `config_parse_channels.c`

**Update (2026-04-06):** Full imsg CLI deep integration — all available imsg features now leveraged:

- **Removed `HU_IMESSAGE_SEND_IMSG` CMake flag.** The `use_imsg_cli` runtime config key is now the single control point. When `true` and `imsg` is on `$PATH`, all imsg features are active.
- **Attachment send via `imsg send --file`**: Each attachment tries `imsg send --to <target> --file <path> --service imessage` first, with per-attachment AppleScript fallback on failure. Faster (<1s) and better error reporting than raw AppleScript.
- **Event-driven polling via `imsg watch`**: On `imessage_start()`, spawns `imsg watch --json --since-rowid <N>` as a background subprocess with pipe. `hu_imessage_poll()` checks the pipe (non-blocking read); skips the SQL query entirely when no new data has arrived. Auto-restarts on process exit. Reduces message delivery latency from up to 3s to sub-second.
- **Target validation via `imsg chats`**: On channel start, runs `imsg chats --json --limit 100` and checks if the configured `default_target` appears. Logs a warning if not found (e.g., typo in phone number). One-time check at startup.
- **Poll interval reduction**: Bootstrap auto-reduces poll interval from 3s to 1s when `use_imsg_cli` is enabled. Combined with watch pipe check, SQL queries only run when there's actually new data.
- **10 new adversarial tests** (Part 14) covering watch lifecycle, null safety, start/stop cycles, send/poll with imsg CLI enabled.
- `hu_imessage_watch_active()` public API for runtime status checking.

### React Path Command Syntax
Corrected to named arguments: `imsg react --chat-id <chat-guid> --message-guid <msg-guid> --reaction <type>`

## Conclusion

**Full integration complete.** All viable imsg v0.5.0 features are now leveraged: text send, attachment send (`--file`), react, event-driven watch, and chat validation. `use_imsg_cli: true` is the recommended production config. The only imsg feature NOT used is `typing` (broken on macOS 26). Every path has graceful fallback to the existing AppleScript/JXA methods when imsg fails or is unavailable.
