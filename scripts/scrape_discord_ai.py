#!/usr/bin/env python3
"""Monitor AI/ML Discord communities via public widget APIs and invite metadata.

Discord doesn't offer public RSS, but we can:
1. Use public widget API for servers with widgets enabled
2. Monitor announcement channels via webhook integrations
3. Track server growth/activity metrics as signals

For full message access, set DISCORD_BOT_TOKEN in ~/.human/config.json feeds.discord.bot_token
and add the bot to target servers.
"""
import json, os, sys, datetime, urllib.request, urllib.error

OUTPUT_DIR = os.path.expanduser("~/.human/feeds/ingest")
OUTPUT_FILE = os.path.join(OUTPUT_DIR, "discord_ai.jsonl")
CONFIG_PATH = os.path.expanduser("~/.human/config.json")

MONITORED_SERVERS = [
    {"id": "974519864045756446", "name": "Midjourney", "focus": "image-generation"},
    {"id": "1009525727504933064", "name": "LangChain", "focus": "agent-frameworks"},
    {"id": "1099885773286969425", "name": "Hugging Face", "focus": "open-source-ml"},
    {"id": "1088874698846986240", "name": "LlamaIndex", "focus": "rag-frameworks"},
    {"id": "1102926478020059197", "name": "OpenAI Community", "focus": "openai-api"},
    {"id": "1151597006481850499", "name": "Anthropic", "focus": "claude-api"},
    {"id": "1067681017716330498", "name": "r/LocalLLaMA", "focus": "local-inference"},
    {"id": "946399602226503691", "name": "Stable Diffusion", "focus": "image-generation"},
    {"id": "1136218227832381523", "name": "Cursor", "focus": "ai-coding"},
    {"id": "1110598183144399038", "name": "Weights & Biases", "focus": "ml-ops"},
    {"id": "823813159592001537", "name": "EleutherAI", "focus": "open-source-llm"},
    {"id": "1055206227587465306", "name": "Together AI", "focus": "inference-infra"},
]

REDDIT_AI_FEEDS = [
    "https://www.reddit.com/r/MachineLearning/top/.json?t=day&limit=5",
    "https://www.reddit.com/r/LocalLLaMA/top/.json?t=day&limit=5",
    "https://www.reddit.com/r/artificial/top/.json?t=day&limit=5",
    "https://www.reddit.com/r/ClaudeAI/top/.json?t=day&limit=5",
    "https://www.reddit.com/r/singularity/top/.json?t=day&limit=5",
    "https://www.reddit.com/r/ChatGPT/top/.json?t=day&limit=5",
    "https://www.reddit.com/r/StableDiffusion/top/.json?t=day&limit=5",
    "https://www.reddit.com/r/Oobabooga/top/.json?t=day&limit=5",
]

DISCORD_WIDGET_API = "https://discord.com/api/guilds/{}/widget.json"

def get_bot_token():
    try:
        with open(CONFIG_PATH) as f:
            cfg = json.load(f)
        return cfg.get("feeds", {}).get("discord", {}).get("bot_token", "")
    except Exception:
        return ""

def fetch_widget(server_id):
    url = DISCORD_WIDGET_API.format(server_id)
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "h-uman-feed/1.0"})
        with urllib.request.urlopen(req, timeout=10) as r:
            return json.loads(r.read())
    except urllib.error.HTTPError:
        return None
    except Exception:
        return None

def fetch_channel_messages(bot_token, channel_id, limit=10):
    url = f"https://discord.com/api/v10/channels/{channel_id}/messages?limit={limit}"
    try:
        req = urllib.request.Request(url, headers={
            "Authorization": f"Bot {bot_token}",
            "User-Agent": "h-uman-feed/1.0",
        })
        with urllib.request.urlopen(req, timeout=10) as r:
            return json.loads(r.read())
    except Exception:
        return []

def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    scrape_ts = datetime.datetime.now(datetime.timezone.utc).isoformat()
    bot_token = get_bot_token()
    items = []

    for server in MONITORED_SERVERS:
        widget = fetch_widget(server["id"])
        if widget:
            online = len(widget.get("members", []))
            channels = widget.get("channels", [])
            channel_names = [c.get("name", "") for c in channels[:5]]

            items.append({
                "source": "discord",
                "content_type": "server_activity",
                "content": f"[{server['name']}] {online} online, channels: {', '.join(channel_names)}. Focus: {server['focus']}"[:2000],
                "url": widget.get("instant_invite", ""),
                "author": server["name"],
                "online_count": online,
                "focus": server["focus"],
                "scraped_at": scrape_ts,
            })

    for reddit_url in REDDIT_AI_FEEDS:
        try:
            req = urllib.request.Request(reddit_url, headers={
                "User-Agent": "h-uman-feed/1.0 (research bot)",
            })
            with urllib.request.urlopen(req, timeout=10) as r:
                data = json.loads(r.read())
            for post in data.get("data", {}).get("children", []):
                pd = post.get("data", {})
                title = pd.get("title", "")
                selftext = pd.get("selftext", "")[:500]
                subreddit = pd.get("subreddit", "")
                score = pd.get("score", 0)
                url = pd.get("url", "")
                items.append({
                    "source": "reddit",
                    "content_type": "reddit_post",
                    "content": f"[r/{subreddit}] {title} (score: {score}){(' - ' + selftext) if selftext else ''}",
                    "url": url,
                    "author": pd.get("author", ""),
                    "subreddit": subreddit,
                    "score": score,
                    "scraped_at": scrape_ts,
                })
        except Exception:
            pass

    if bot_token:
        with open(CONFIG_PATH) as f:
            cfg = json.load(f)
        watch_channels = cfg.get("feeds", {}).get("discord", {}).get("watch_channels", [])
        for ch in watch_channels:
            channel_id = ch.get("id", "")
            channel_name = ch.get("name", "unknown")
            if not channel_id:
                continue
            messages = fetch_channel_messages(bot_token, channel_id, limit=5)
            for msg in messages:
                content = msg.get("content", "")
                if not content:
                    continue
                author = msg.get("author", {}).get("username", "")
                items.append({
                    "source": "discord",
                    "content_type": "message",
                    "content": f"[{channel_name}] {author}: {content}"[:2000],
                    "url": "",
                    "author": author,
                    "channel": channel_name,
                    "scraped_at": scrape_ts,
                })

    with open(OUTPUT_FILE, "w") as f:
        for item in items:
            f.write(json.dumps(item) + "\n")

    mode = "bot + widget" if bot_token else "widget-only"
    print(f"[discord] {len(items)} items ({mode}) -> {OUTPUT_FILE}")

if __name__ == "__main__":
    main()
