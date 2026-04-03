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

Our current AppleScript send path is stable and well-tested. The imsg send path is now implemented as an **opt-in alternative** via `HU_IMESSAGE_SEND_IMSG` CMake flag, not a replacement.

**Implementation (2026-04-03):** `HU_IMESSAGE_SEND_IMSG` CMake option added. When enabled:
- Text sends try `imsg send --to <target> --text <message> --service imessage` first
- Attachment sends use `imsg send --to <target> --file <path> --service imessage`
- Graceful fallback: if imsg fails, falls through to existing AppleScript path
- Media-only sends (no text) also support imsg with AppleScript fallback

### React Path Command Syntax
Corrected to named arguments: `imsg react --chat-id <chat-guid> --message-guid <msg-guid> --reaction <type>`

## Conclusion

**Positive evaluation.** imsg v0.5.0 is a viable, lower-risk alternative for both tapback sending and message sending. It eliminates the Accessibility permission requirement for tapbacks, works headless, and supports custom emoji reactions. Both the react and send paths are implemented as opt-in with graceful fallback to existing methods. Typing indicators are broken and should not be pursued.
