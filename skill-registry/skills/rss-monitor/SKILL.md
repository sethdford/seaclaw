---
name: rss-monitor
description: Monitor RSS feeds and notify on new posts
---

# Rss Monitor

Track feeds for new items without hammering publishers. Honor `ETag`, `Last-Modified`, and crawl delays; dedupe by `guid`/`link`.

Handle malformed XML gracefully; surface permanent feed failures.

## When to Use
- News digests, release monitoring, or competitor blog tracking

## Workflow
1. Normalize URLs; validate feed parse; store last seen ids.
2. Poll at polite intervals; exponential backoff on errors.
3. Summarize new entries with title, source, date, and link.
4. Alert on feed death (404/parse errors) for maintenance.

## Examples
**Example 1:** Security advisories: filter by product keywords; high-priority channel.

**Example 2:** Podcast feed: new enclosure URL triggers download job.
