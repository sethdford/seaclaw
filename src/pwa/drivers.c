/*
 * PWA Drivers — per-app selectors and JavaScript snippets for common web apps.
 * Each driver maps an app name to CSS selectors and JS action templates.
 *
 * Drivers work by injecting JavaScript into the app's DOM to:
 *   - Read visible messages (scrape text from message containers)
 *   - Send messages (focus input, set value, dispatch events)
 *   - Navigate to channels/contacts
 *
 * The %s placeholder in send_message_js is replaced with the escaped message.
 * The %s placeholder in navigate_js is replaced with the escaped target name.
 */
#include "human/pwa.h"
#include <string.h>

/* ── Slack ──────────────────────────────────────────────────────────── */

static const hu_pwa_driver_t DRIVER_SLACK = {
    .app_name = "slack",
    .display_name = "Slack",
    .url_pattern = "app.slack.com",

    .read_messages_js =
        "(function(){"
        "  var msgs = document.querySelectorAll('[data-qa=\"virtual-list-item\"]');"
        "  var out = [];"
        "  msgs.forEach(function(m){"
        "    var sender = m.querySelector('[data-qa=\"message_sender_name\"]');"
        "    var body = m.querySelector('.p-rich_text_section');"
        "    if(body) out.push((sender?sender.textContent:'?') + ': ' + body.textContent);"
        "  });"
        "  return out.slice(-20).join('\\n');"
        "})()",

    .send_message_js =
        "(function(){"
        "  var el = document.querySelector('[data-qa=\"message_input\"] .ql-editor, "
        "[contenteditable=\"true\"][data-qa=\"message_input\"]');"
        "  if(!el) return 'ERROR: input not found';"
        "  el.focus();"
        "  document.execCommand('insertText', false, '%s');"
        "  setTimeout(function(){"
        "    var ev = new KeyboardEvent('keydown',{key:'Enter',code:'Enter',keyCode:13,bubbles:true});"
        "    el.dispatchEvent(ev);"
        "  }, 100);"
        "  return 'sent';"
        "})()",

    .read_contacts_js =
        "(function(){"
        "  var items = document.querySelectorAll('.p-channel_sidebar__channel');"
        "  var out = [];"
        "  items.forEach(function(i){"
        "    var name = i.querySelector('.p-channel_sidebar__name');"
        "    if(name) out.push(name.textContent.trim());"
        "  });"
        "  return out.join('\\n');"
        "})()",

    .navigate_js =
        "(function(){"
        "  var items = document.querySelectorAll('.p-channel_sidebar__channel');"
        "  for(var i=0;i<items.length;i++){"
        "    var name = items[i].querySelector('.p-channel_sidebar__name');"
        "    if(name && name.textContent.trim().toLowerCase().indexOf('%s'.toLowerCase())>=0){"
        "      items[i].click(); return 'navigated';"
        "    }"
        "  }"
        "  return 'channel not found';"
        "})()",
};

/* ── Discord ───────────────────────────────────────────────────────── */

static const hu_pwa_driver_t DRIVER_DISCORD = {
    .app_name = "discord",
    .display_name = "Discord",
    .url_pattern = "discord.com",

    .read_messages_js =
        "(function(){"
        "  var msgs = document.querySelectorAll('[id^=\"chat-messages-\"]');"
        "  var out = [];"
        "  msgs.forEach(function(m){"
        "    var header = m.querySelector('[class*=\"username\"]');"
        "    var body = m.querySelector('[id^=\"message-content-\"]');"
        "    if(body) out.push((header?header.textContent:'?') + ': ' + body.textContent);"
        "  });"
        "  return out.slice(-20).join('\\n');"
        "})()",

    .send_message_js =
        "(function(){"
        "  var el = document.querySelector('[role=\"textbox\"][data-slate-editor]');"
        "  if(!el) return 'ERROR: input not found';"
        "  el.focus();"
        "  document.execCommand('insertText', false, '%s');"
        "  setTimeout(function(){"
        "    var ev = new KeyboardEvent('keydown',{key:'Enter',code:'Enter',keyCode:13,bubbles:true});"
        "    el.dispatchEvent(ev);"
        "  }, 100);"
        "  return 'sent';"
        "})()",

    .read_contacts_js = NULL,

    .navigate_js =
        "(function(){"
        "  var channels = document.querySelectorAll('[data-list-item-id^=\"channels___\"]');"
        "  for(var i=0;i<channels.length;i++){"
        "    if(channels[i].textContent.toLowerCase().indexOf('%s'.toLowerCase())>=0){"
        "      channels[i].click(); return 'navigated';"
        "    }"
        "  }"
        "  return 'channel not found';"
        "})()",
};

/* ── WhatsApp ──────────────────────────────────────────────────────── */

static const hu_pwa_driver_t DRIVER_WHATSAPP = {
    .app_name = "whatsapp",
    .display_name = "WhatsApp",
    .url_pattern = "web.whatsapp.com",

    .read_messages_js =
        "(function(){"
        "  var msgs = document.querySelectorAll('.message-in, .message-out');"
        "  var out = [];"
        "  msgs.forEach(function(m){"
        "    var body = m.querySelector('.copyable-text');"
        "    var dir = m.classList.contains('message-out') ? 'You' : 'Them';"
        "    if(body) out.push(dir + ': ' + body.textContent);"
        "  });"
        "  return out.slice(-20).join('\\n');"
        "})()",

    .send_message_js =
        "(function(){"
        "  var el = document.querySelector('footer [contenteditable=\"true\"],"
        " div[contenteditable=\"true\"][data-tab=\"10\"]');"
        "  if(!el) return 'ERROR: input not found';"
        "  el.focus();"
        "  document.execCommand('insertText', false, '%s');"
        "  setTimeout(function(){"
        "    var btn = document.querySelector('[data-icon=\"send\"], [aria-label=\"Send\"]');"
        "    if(btn) btn.click();"
        "  }, 200);"
        "  return 'sent';"
        "})()",

    .read_contacts_js =
        "(function(){"
        "  var items = document.querySelectorAll('[data-testid=\"cell-frame-container\"]');"
        "  var out = [];"
        "  items.forEach(function(i){"
        "    var name = i.querySelector('span[title]');"
        "    if(name) out.push(name.getAttribute('title'));"
        "  });"
        "  return out.slice(0, 30).join('\\n');"
        "})()",

    .navigate_js =
        "(function(){"
        "  var search = document.querySelector('[data-testid=\"chat-list-search\"],"
        " [contenteditable=\"true\"][data-tab=\"3\"]');"
        "  if(!search) return 'search not found';"
        "  search.focus();"
        "  document.execCommand('selectAll');"
        "  document.execCommand('insertText', false, '%s');"
        "  return 'searching';"
        "})()",
};

/* ── Gmail ──────────────────────────────────────────────────────────── */

static const hu_pwa_driver_t DRIVER_GMAIL = {
    .app_name = "gmail",
    .display_name = "Gmail",
    .url_pattern = "mail.google.com",

    .read_messages_js =
        "(function(){"
        "  var rows = document.querySelectorAll('tr.zA');"
        "  var out = [];"
        "  rows.forEach(function(r){"
        "    var from = r.querySelector('.yW span');"
        "    var subj = r.querySelector('.bog span');"
        "    var snip = r.querySelector('.y2');"
        "    var unread = r.classList.contains('zE') ? '[NEW] ' : '';"
        "    if(subj) out.push(unread + (from?from.textContent:'?') + ' — ' + subj.textContent + (snip?' — '+snip.textContent:''));"
        "  });"
        "  return out.slice(0, 20).join('\\n');"
        "})()",

    .send_message_js =
        "(function(){"
        "  var body = document.querySelector('.Am.Al.editable[contenteditable=\"true\"]');"
        "  if(!body) return 'ERROR: compose body not found — click Compose first';"
        "  body.focus();"
        "  document.execCommand('insertText', false, '%s');"
        "  return 'typed into compose body';"
        "})()",

    .read_contacts_js = NULL,

    .navigate_js =
        "(function(){"
        "  var search = document.querySelector('input[aria-label=\"Search mail\"]');"
        "  if(!search) return 'search not found';"
        "  search.focus();"
        "  search.value = '%s';"
        "  search.dispatchEvent(new Event('input', {bubbles:true}));"
        "  var form = search.closest('form');"
        "  if(form) form.submit();"
        "  return 'searching';"
        "})()",
};

/* ── Google Calendar ───────────────────────────────────────────────── */

static const hu_pwa_driver_t DRIVER_CALENDAR = {
    .app_name = "calendar",
    .display_name = "Google Calendar",
    .url_pattern = "calendar.google.com",

    .read_messages_js =
        "(function(){"
        "  var events = document.querySelectorAll('[data-eventchip]');"
        "  var out = [];"
        "  events.forEach(function(e){"
        "    out.push(e.textContent.trim());"
        "  });"
        "  return out.slice(0, 30).join('\\n') || 'No visible events';"
        "})()",

    .send_message_js = NULL,
    .read_contacts_js = NULL,
    .navigate_js = NULL,
};

/* ── Notion ────────────────────────────────────────────────────────── */

static const hu_pwa_driver_t DRIVER_NOTION = {
    .app_name = "notion",
    .display_name = "Notion",
    .url_pattern = "notion.so",

    .read_messages_js =
        "(function(){"
        "  var blocks = document.querySelectorAll('[data-block-id] .notion-text-block,"
        " [data-block-id] .notion-header-block,"
        " [data-block-id] .notion-sub_header-block,"
        " [data-block-id] .notion-to_do-block');"
        "  var out = [];"
        "  blocks.forEach(function(b){ out.push(b.textContent.trim()); });"
        "  return out.slice(0, 40).join('\\n') || 'Empty page';"
        "})()",

    .send_message_js =
        "(function(){"
        "  var blocks = document.querySelectorAll('[contenteditable=\"true\"]');"
        "  var last = blocks[blocks.length - 1];"
        "  if(!last) return 'ERROR: no editable block found';"
        "  last.focus();"
        "  document.execCommand('insertText', false, '%s');"
        "  return 'typed';"
        "})()",

    .read_contacts_js = NULL,

    .navigate_js =
        "(function(){"
        "  var items = document.querySelectorAll('.notion-sidebar-item');"
        "  for(var i=0;i<items.length;i++){"
        "    if(items[i].textContent.toLowerCase().indexOf('%s'.toLowerCase())>=0){"
        "      items[i].click(); return 'navigated';"
        "    }"
        "  }"
        "  return 'page not found';"
        "})()",
};

/* ── Twitter/X ─────────────────────────────────────────────────────── */

static const hu_pwa_driver_t DRIVER_TWITTER = {
    .app_name = "twitter",
    .display_name = "Twitter/X",
    .url_pattern = "x.com",

    .read_messages_js =
        "(function(){"
        "  var tweets = document.querySelectorAll('[data-testid=\"tweet\"]');"
        "  var out = [];"
        "  tweets.forEach(function(t){"
        "    var user = t.querySelector('[data-testid=\"User-Name\"]');"
        "    var text = t.querySelector('[data-testid=\"tweetText\"]');"
        "    if(text) out.push((user?user.textContent:'?') + ': ' + text.textContent);"
        "  });"
        "  return out.slice(0, 15).join('\\n');"
        "})()",

    .send_message_js =
        "(function(){"
        "  var el = document.querySelector('[data-testid=\"dmComposerTextInput\"],"
        " [data-testid=\"tweetTextarea_0\"]');"
        "  if(!el) return 'ERROR: input not found';"
        "  el.focus();"
        "  document.execCommand('insertText', false, '%s');"
        "  return 'typed';"
        "})()",

    .read_contacts_js = NULL,
    .navigate_js = NULL,
};

/* ── Telegram Web ──────────────────────────────────────────────────── */

static const hu_pwa_driver_t DRIVER_TELEGRAM = {
    .app_name = "telegram",
    .display_name = "Telegram",
    .url_pattern = "web.telegram.org",

    .read_messages_js =
        "(function(){"
        "  var msgs = document.querySelectorAll('.Message');"
        "  var out = [];"
        "  msgs.forEach(function(m){"
        "    var sender = m.querySelector('.message-title');"
        "    var body = m.querySelector('.text-content');"
        "    if(body) out.push((sender?sender.textContent:'?') + ': ' + body.textContent);"
        "  });"
        "  return out.slice(-20).join('\\n');"
        "})()",

    .send_message_js =
        "(function(){"
        "  var el = document.querySelector('#editable-message-text, .input-message-input');"
        "  if(!el) return 'ERROR: input not found';"
        "  el.focus();"
        "  document.execCommand('insertText', false, '%s');"
        "  setTimeout(function(){"
        "    var btn = document.querySelector('.send-btn, .Button.send');"
        "    if(btn) btn.click();"
        "  }, 200);"
        "  return 'sent';"
        "})()",

    .read_contacts_js = NULL,

    .navigate_js =
        "(function(){"
        "  var search = document.querySelector('#telegram-search-input, .input-search input');"
        "  if(!search) return 'search not found';"
        "  search.focus();"
        "  search.value = '%s';"
        "  search.dispatchEvent(new Event('input', {bubbles:true}));"
        "  return 'searching';"
        "})()",
};

/* ── LinkedIn ──────────────────────────────────────────────────────── */

static const hu_pwa_driver_t DRIVER_LINKEDIN = {
    .app_name = "linkedin",
    .display_name = "LinkedIn",
    .url_pattern = "linkedin.com",

    .read_messages_js =
        "(function(){"
        "  var msgs = document.querySelectorAll('.msg-s-event-listitem');"
        "  var out = [];"
        "  msgs.forEach(function(m){"
        "    var sender = m.querySelector('.msg-s-message-group__name');"
        "    var body = m.querySelector('.msg-s-event-listitem__body');"
        "    if(body) out.push((sender?sender.textContent.trim():'?') + ': ' + body.textContent.trim());"
        "  });"
        "  return out.slice(-15).join('\\n');"
        "})()",

    .send_message_js =
        "(function(){"
        "  var el = document.querySelector('.msg-form__contenteditable[contenteditable=\"true\"]');"
        "  if(!el) return 'ERROR: input not found';"
        "  el.focus();"
        "  document.execCommand('insertText', false, '%s');"
        "  setTimeout(function(){"
        "    var btn = document.querySelector('.msg-form__send-button');"
        "    if(btn) btn.click();"
        "  }, 200);"
        "  return 'sent';"
        "})()",

    .read_contacts_js = NULL,
    .navigate_js = NULL,
};

/* ── Driver Registry ───────────────────────────────────────────────── */

static const hu_pwa_driver_t *const ALL_DRIVERS[] = {
    &DRIVER_SLACK,
    &DRIVER_DISCORD,
    &DRIVER_WHATSAPP,
    &DRIVER_GMAIL,
    &DRIVER_CALENDAR,
    &DRIVER_NOTION,
    &DRIVER_TWITTER,
    &DRIVER_TELEGRAM,
    &DRIVER_LINKEDIN,
};

#define DRIVER_COUNT (sizeof(ALL_DRIVERS) / sizeof(ALL_DRIVERS[0]))

const hu_pwa_driver_t *hu_pwa_driver_find(const char *app_name) {
    if (!app_name)
        return NULL;
    for (size_t i = 0; i < DRIVER_COUNT; i++) {
        if (strcmp(ALL_DRIVERS[i]->app_name, app_name) == 0)
            return ALL_DRIVERS[i];
    }
    return NULL;
}

const hu_pwa_driver_t *hu_pwa_driver_find_by_url(const char *url) {
    if (!url)
        return NULL;
    for (size_t i = 0; i < DRIVER_COUNT; i++) {
        if (strstr(url, ALL_DRIVERS[i]->url_pattern))
            return ALL_DRIVERS[i];
    }
    return NULL;
}

const hu_pwa_driver_t *hu_pwa_drivers_all(size_t *count) {
    if (count)
        *count = DRIVER_COUNT;
    return ALL_DRIVERS[0];
}
