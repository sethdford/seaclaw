# IMAP channel with Gmail

The `imap` channel in `human` uses **libcurl** for **IMAP** (poll) and **SMTP** (send) when built with `HU_ENABLE_CURL` and `HU_HTTP_CURL`.

## Gmail requirements

- **IMAP**: `imap.gmail.com`, port **993**, TLS on (`imap_use_tls`: true in JSON).
- **SMTP**: `smtp.gmail.com`, port **587** (STARTTLS).
- **Auth**: Use a **[Google App Password](https://support.google.com/accounts/answer/185833)** for the account password in config — not your normal Google password, if 2-Step Verification is enabled.
- **“Less secure apps”** is deprecated; prefer app passwords (or OAuth, which this channel does not implement yet).

## Example `config.json` fragment

```json
{
  "channels": {
    "imap": {
      "imap_host": "imap.gmail.com",
      "imap_port": 993,
      "imap_username": "you@gmail.com",
      "imap_password": "your-app-password",
      "imap_folder": "INBOX",
      "imap_use_tls": true,
      "smtp_host": "smtp.gmail.com",
      "smtp_port": 587,
      "from_address": "you@gmail.com"
    }
  }
}
```

Use the same app password for both IMAP and SMTP; the channel passes `CURLOPT_USERNAME` / `CURLOPT_PASSWORD` to libcurl for both protocols.

## One-shot integration test (local)

From the repo root (never commit credentials):

```bash
export HU_INTEG_GMAIL_USER='you@gmail.com'
export HU_INTEG_GMAIL_APP_PASS='xxxx xxxx xxxx xxxx'
bash scripts/integ-imap-gmail.sh
```

This sends a short test line to `HU_INTEG_IMAP_TO` (defaults to your address), then polls until the body appears or times out with **SKIP** if Gmail does not expose the message as **UNSEEN** quickly enough.

## Non-Gmail local proof

For CI-style proof without Gmail, use Docker + GreenMail:

```bash
bash scripts/integ-imap-local.sh
```

## Related

- **Gmail API channel** (`channels.gmail` / `src/channels/gmail.c`) is separate from this **IMAP** channel; use one strategy per deployment to avoid duplicate ingestion.
