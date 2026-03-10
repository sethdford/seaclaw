---
title: iMessage Edit Feasibility Investigation
date: 2026-03-08
feature: F41 — Message editing
status: Partially Feasible
---

# iMessage Edit Feasibility Investigation

**Date:** 2026-03-08  
**Feature:** F41 — Message editing  
**Status:** Partially Feasible

## Summary

Editing sent iMessages is **not possible via public APIs** (AppleScript, JXA). It **is possible** via Apple's private IMCore framework (`[chat editMessage:atPartIndex:withNewPartText:backwardCompatabilityText:]`), as demonstrated by BlueBubbles. However, private API use is risky for production: it breaks on macOS updates and violates Apple's terms. **Recommendation:** Retain the `*correction` pattern for the human project; do not adopt private API editing.

## Research Findings

### AppleScript Messages Dictionary

The Messages.app AppleScript dictionary does **not** expose any "edit" or "replace" verb for modifying sent messages.

- **Available:** `send` — send a new message to a buddy/service.
- **Not available:** No `edit`, `replace`, `modify`, or equivalent verb for existing messages.
- **Source:** AppleScript automation for Messages is limited to sending; the dictionary does not expose message objects for modification. Community discussions (Apple Developer Forums, Stack Overflow) confirm this as of 2024.

### JavaScript for Automation (JXA)

JXA cannot edit sent iMessages.

- JXA uses the same scripting interface as AppleScript; it only exposes what the Messages app dictionary provides.
- JXA can `send` messages but has no access to message objects or edit operations.
- Native iMessage editing (iOS 16+, macOS Ventura) is a **manual GUI operation** (right-click → Edit within 15 minutes). It is not exposed to automation.
- **Conclusion:** No programmatic edit path via JXA.

### Private Frameworks

Apple's private IMCore framework **does** expose edit functionality.

- **Method:** `[IMChat editMessage:(IMMessage *)message atPartIndex:(NSInteger)index withNewPartText:(NSAttributedString *)newText backwardCompatabilityText:(NSAttributedString *)bcText]`
- **Requirements:** macOS 13+ (Ventura). The method exists in IMCore and is used by BlueBubbles' helper bundle.
- **Implementation details (from BlueBubbles docs):**
  - Obtain `IMChat` via `IMChatRegistry` (by GUID).
  - Obtain `IMMessage` via `IMChatHistoryController loadMessageWithGUID:completionBlock:`.
  - `partIndex` identifies which part of a multi-part message (e.g., text + attachment) to edit; indexes start at 0.
  - `backwardCompatabilityText` is a plain-text fallback for older clients.
- **Risks:**
  - Private API — undocumented, subject to change or removal on any macOS update.
  - Violates Apple's terms of service; not suitable for App Store or production distribution.
  - Requires Objective-C/Swift + dylib injection or helper process; not feasible from C runtime without significant bridging.

### Community Tools

| Tool                       | Edit Support  | Notes                                                                                           |
| -------------------------- | ------------- | ----------------------------------------------------------------------------------------------- |
| **golift/imessage**        | No            | Go library for send/read; no edit API.                                                          |
| **imessage_tools**         | No            | Read chat.db, send messages; no edit.                                                           |
| **steipete/imsg**          | No            | Reactions, typing indicators; no edit in releases.                                              |
| **photon-hq/imessage-kit** | No (standard) | Edit/unsend listed as _not_ in standard SDK; "Advanced iMessage Kit" may offer it (commercial). |
| **BlueBubbles**            | Yes           | Uses IMCore private API; edit implemented in helper bundle.                                     |

**Conclusion:** Only BlueBubbles (private API) implements edit. Open-source tools do not.

### Direct Database Modification

Modifying `~/Library/Messages/chat.db` directly is **not viable** for editing messages.

- **Schema:** `message` table holds `text`, `attributedBody`, etc. In Ventura+, `attributedBody` often stores hex-encoded blobs instead of plain text.
- **Risks:**
  - **No iCloud sync:** Local edits do not propagate to iCloud or recipient devices.
  - **Integrity:** Messages are tied to GUIDs, timestamps, and sync metadata; manual edits can corrupt state.
  - **Format:** `attributedBody` requires parsing/serialization; direct SQL updates are error-prone.
- **Conclusion:** Not recommended. At best, local-only display change; recipient sees original. Not equivalent to iOS 16+ edit.

## Recommendation

**Do not implement iMessage editing** for the human project.

1. **Public API:** No path exists. AppleScript and JXA cannot edit messages.
2. **Private API:** Technically possible via IMCore, but:
   - Breaks on macOS updates.
   - Not suitable for production C runtime.
   - Violates Apple's terms.
3. **chat.db:** Does not sync; does not replicate native edit behavior.

**Retain the `*correction` pattern** as the fallback. It is:

- Reliable across macOS versions.
- Implemented in `hu_conversation_generate_correction()` and wired in `daemon.c`.
- Human-like (e.g., `*meeting` after a typo) and does not depend on undocumented APIs.

## Fallback

**`*correction` pattern** — when a typo is introduced (e.g., "meating" instead of "meeting"), send a follow-up correction fragment:

- Format: `*<correct_word>` (e.g., `*meeting`)
- Logic: `hu_conversation_generate_correction(original, typo_applied, out_buf, ...)` detects first differing word and emits correction with configurable probability.
- Placement: Sent 2.5–5s after main message to mimic natural self-correction timing.
- Location: `src/context/conversation.c`, `src/daemon.c` (BTH typo pipeline).

This pattern is already implemented and tested. No code changes required for F41; investigation complete.
