---
title: "iMessage Sticker/Memoji Feasibility Investigation"
description: "Feasibility of reading and sending stickers and Memoji via chat.db and available APIs."
---

# iMessage Sticker/Memoji Feasibility Investigation

**Date:** 2026-04-03  
**Status:** Investigation  
**Related:** `imessage-capability-matrix.md`

## Background

User behavior data shows 1B+ Memoji stickers sent weekly (Apple WWDC data) and ~40% of users have created a Memoji. Our iMessage channel currently has zero sticker/Memoji support — we cannot detect when someone sends a sticker, nor can we send one.

## Reading Stickers from chat.db

### How Stickers Appear

Sticker messages in chat.db are characterized by:
- **`balloon_bundle_id`**: Contains the sticker pack identifier (e.g., `com.apple.Stickers.UserGenerated.StickerPack`, `com.apple.messages.MSMessageExtensionBalloonPlugin:*`)
- **`text`**: Usually NULL or empty for sticker-only messages
- **`attributedBody`**: May contain minimal metadata but not the visual content
- **Attachment**: The sticker image is stored as an attachment (linked via `message_attachment_join` → `attachment` table), typically as a PNG or HEIC file

### Detection Strategy

To detect sticker messages in our poll:
1. Check `balloon_bundle_id` — if it starts with `com.apple.Stickers` or is a known sticker extension identifier, it's a sticker
2. The attachment table has the file path and MIME type for the sticker image
3. Memoji stickers specifically use identifiers related to the Memoji/Animoji system

### SQL Query for Sticker Detection

```sql
SELECT m.ROWID, m.guid, m.balloon_bundle_id, m.text, a.filename, a.mime_type
FROM message m
LEFT JOIN message_attachment_join maj ON maj.message_id = m.ROWID
LEFT JOIN attachment a ON a.ROWID = maj.attachment_id
WHERE m.balloon_bundle_id IS NOT NULL
  AND m.balloon_bundle_id LIKE '%Sticker%'
ORDER BY m.date DESC LIMIT 10;
```

### Feasibility: **Feasible (read-side)**
We can detect stickers in poll output by checking `balloon_bundle_id`. Implementation would add a `[They sent a sticker]` or `[Sticker: <pack-name>]` placeholder in the message content, similar to how we handle other non-text content types.

## Sending Stickers

### Investigation

There is no public API to send a sticker via AppleScript, JXA, or any known CLI tool:
- AppleScript `send` only supports text and file attachments
- imsg v0.5.0 has no sticker send command
- BlueBubbles does not support sticker sending either
- The Messages app sticker picker is a UI-only feature
- Memoji stickers are generated client-side in the Messages app

### Could We Send a Sticker-Like Image?
We could potentially send a static image (PNG) that resembles a sticker via the attachment path. However:
- It would appear as a regular image, not as a sticker (different bubble rendering)
- We cannot trigger the sticker-specific iMessage extension behavior
- Memoji generation requires the Memoji SDK (iOS/macOS only, no public API for headless generation)

### Feasibility: **Not Feasible (send-side)**
Sending actual stickers is not possible via any public or third-party API.

## User Impact Assessment

| Scenario | Frequency | Impact |
|----------|-----------|--------|
| They send a sticker, we see nothing | Common (40%+ users use) | Agent may respond to wrong context or seem confused |
| They send a Memoji reaction | Common | Agent misses emotional signal |
| Agent cannot send stickers | Always | Limits expressiveness but acceptable — text alternatives work |

The read-side gap is more impactful than the send-side limitation. Users who send stickers expect acknowledgment. Currently, our poll sees an empty or null message, which may cause the agent to:
- Ignore the message entirely (if text is NULL/empty)
- Respond to the previous message instead

## Recommendation

### Phase 1: Detect stickers in poll (read-side)
1. In `hu_imessage_poll()`, check `balloon_bundle_id` when text is NULL/empty
2. If it matches a sticker pattern, set message content to `[They sent a sticker]`
3. Optionally, include attachment info: `[They sent a sticker (filename.png)]`
4. This gives the agent context to respond appropriately (e.g., acknowledge with a tapback)

### Phase 2: Not planned (send-side)
Sending stickers is not feasible. The agent can use emoji, tapbacks, and text alternatives to express similar sentiment.

## Conclusion

**Read-side sticker detection is feasible and recommended.** The `balloon_bundle_id` field provides reliable identification. Send-side sticker support is not possible via any public API. Priority: Medium — addresses a real user experience gap in conversations with sticker-heavy users.

---

## Update (2026-04-04)

**Phase 1 (read-side sticker detection) is now implemented:**

- `hu_imessage_poll()` now reads `balloon_bundle_id` from chat.db (column 11 in the poll query)
- Detection logic: if text is NULL/empty and `balloon_bundle_id` is present:
  - Contains "Sticker" or "sticker" → content set to `[Sticker]`
  - Contains "Animoji", "animoji", "Memoji", or "memoji" → content set to `[Memoji]`
  - Other non-NULL → content set to `[iMessage App]`
- WHERE clause updated to include `OR (m.balloon_bundle_id IS NOT NULL)` to catch sticker messages
- Agent now has context to respond appropriately to sticker messages

Phase 2 (send-side) remains not feasible. No change from original conclusion.
