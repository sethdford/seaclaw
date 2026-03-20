---
name: calendar-sync
description: Sync and manage calendar events across providers
---

# Calendar Sync

Keep calendars consistent across providers without duplicate or dropped events. Mind time zones, recurrence exceptions, and attendee privacy.

Prefer idempotent sync jobs and conflict rules (e.g., source-of-truth calendar wins).

## When to Use
- Multi-calendar users, team visibility, or migration between Google/Microsoft/Apple

## Workflow
1. List calendars, read/write permissions, and canonical timezone.
2. Map event fields (UID, recurrence, attendees); define dedupe keys.
3. Implement incremental sync with cursors or timestamps; log conflicts.
4. Dry-run on a test calendar before mass updates.

## Examples
**Example 1:** Block personal “focus” slots on work calendar without exposing titles.

**Example 2:** Import ICS export: skip duplicates by `UID` match.
