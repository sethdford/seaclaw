#include "human/agent/conversation_plan.h"
#include "human/core/string.h"
#include "human/data/loader.h"
#include "human/core/json.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#define TARGET_LENGTH_CAP           300
#define DEEP_CONVERSATION_THRESHOLD 1000

/* Default fallback arrays (NULL-terminated) */
static const char *DEFAULT_EMOTIONAL_KEYWORDS[] = {
    "sad",          "happy",    "angry",       "frustrated", "worried", "anxious", "excited",
    "disappointed", "stressed", "overwhelmed", "lonely",     "scared",  "hurt",
    NULL
};

static const char *DEFAULT_NEW_TOPIC_PATTERNS[] = {
    "new ", "about ", "tell me", "what about", "regarding ",
    NULL
};

/* Runtime loaded patterns */
static const char **s_emotional_keywords = DEFAULT_EMOTIONAL_KEYWORDS;
static const char **s_new_topic_patterns = DEFAULT_NEW_TOPIC_PATTERNS;

static void load_patterns(hu_allocator_t *alloc, hu_json_value_t *root, const char *key,
                          const char ***dest) {
    if (!root || !dest)
        return;
    hu_json_value_t *arr = hu_json_object_get(root, key);
    if (!arr || arr->type != HU_JSON_ARRAY)
        return;
    size_t count = arr->data.array.len;
    if (count == 0)
        return;
    const char **patterns = (const char **)alloc->alloc(alloc->ctx, (count + 1) * sizeof(const char *));
    if (!patterns)
        return;
    memset(patterns, 0, (count + 1) * sizeof(const char *));
    for (size_t i = 0; i < count; i++) {
        hu_json_value_t *item = arr->data.array.items[i];
        if (item && item->type == HU_JSON_STRING) {
            patterns[i] = hu_strndup(alloc, item->data.string.ptr, item->data.string.len);
        }
    }
    patterns[count] = NULL;
    *dest = patterns;
}

hu_error_t hu_conversation_plan_data_init(hu_allocator_t *alloc) {
    if (!alloc)
        return HU_ERR_INVALID_ARGUMENT;

    char *json_data = NULL;
    size_t json_len = 0;
    hu_error_t err = hu_data_load(alloc, "agent/conversation_plan_words.json", &json_data, &json_len);
    if (err != HU_OK)
        return HU_OK; /* Fail gracefully, keep defaults */

    hu_json_value_t *root = NULL;
    err = hu_json_parse(alloc, json_data, json_len, &root);
    alloc->free(alloc->ctx, json_data, json_len);
    if (err != HU_OK || !root)
        return HU_OK; /* Fail gracefully, keep defaults */

    load_patterns(alloc, root, "emotional_keywords", &s_emotional_keywords);
    load_patterns(alloc, root, "new_topic_patterns", &s_new_topic_patterns);

    hu_json_free(alloc, root);
    return HU_OK;
}

static void free_patterns(hu_allocator_t *alloc, const char **arr, const char **default_arr) {
    if (arr != default_arr && arr) {
        for (size_t i = 0; arr[i]; i++) {
            alloc->free(alloc->ctx, (char *)arr[i], strlen(arr[i]) + 1);
        }
        size_t count = 0;
        for (size_t i = 0; arr[i]; i++) count++;
        alloc->free(alloc->ctx, (void *)arr, (count + 1) * sizeof(const char *));
    }
}

void hu_conversation_plan_data_cleanup(hu_allocator_t *alloc) {
    if (!alloc)
        return;

    free_patterns(alloc, s_emotional_keywords, DEFAULT_EMOTIONAL_KEYWORDS);
    free_patterns(alloc, s_new_topic_patterns, DEFAULT_NEW_TOPIC_PATTERNS);

    s_emotional_keywords = DEFAULT_EMOTIONAL_KEYWORDS;
    s_new_topic_patterns = DEFAULT_NEW_TOPIC_PATTERNS;
}

static bool contains_emotional_keyword(const char *msg, size_t len) {
    for (size_t i = 0; s_emotional_keywords[i]; i++) {
        const char *kw = s_emotional_keywords[i];
        size_t kw_len = strlen(kw);
        for (size_t j = 0; j + kw_len <= len; j++) {
            if (strncasecmp(msg + j, kw, kw_len) != 0)
                continue;
            bool start_ok = (j == 0) || !isalnum((unsigned char)msg[j - 1]);
            bool end_ok = (j + kw_len >= len) || !isalnum((unsigned char)msg[j + kw_len]);
            if (start_ok && end_ok)
                return true;
        }
    }
    return false;
}

static bool contains_question(const char *msg, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (msg[i] == '?')
            return true;
    }
    return false;
}

static bool mentions_new_topic(const char *msg, size_t len) {
    for (size_t p = 0; s_new_topic_patterns[p]; p++) {
        size_t plen = strlen(s_new_topic_patterns[p]);
        for (size_t i = 0; i + plen <= len; i++) {
            if (strncasecmp(msg + i, s_new_topic_patterns[p], plen) == 0)
                return true;
        }
    }
    return false;
}

static const char *intent_name(hu_plan_intent_t intent) {
    switch (intent) {
    case HU_PLAN_RESPOND:
        return "respond";
    case HU_PLAN_REDIRECT:
        return "redirect";
    case HU_PLAN_DEEPEN:
        return "deepen";
    case HU_PLAN_LIGHTEN:
        return "lighten";
    case HU_PLAN_VALIDATE:
        return "validate";
    case HU_PLAN_INFORM:
        return "inform";
    default:
        return "respond";
    }
}

hu_error_t hu_plan_conversation(hu_allocator_t *alloc, const char *user_message,
                                size_t user_msg_len,
                                const char *conversation_history __attribute__((unused)),
                                size_t history_len, const char *emotional_context,
                                size_t emotional_len, hu_conversation_plan_t *plan) {
    if (!alloc || !plan)
        return HU_ERR_INVALID_ARGUMENT;

    memset(plan, 0, sizeof(*plan));

    plan->primary_intent = HU_PLAN_RESPOND;

    if (user_message && user_msg_len > 0) {
        if (user_msg_len < 20 && contains_emotional_keyword(user_message, user_msg_len))
            plan->primary_intent = HU_PLAN_VALIDATE;
        else if (mentions_new_topic(user_message, user_msg_len))
            plan->primary_intent = HU_PLAN_INFORM;
        else if (contains_question(user_message, user_msg_len))
            plan->primary_intent = HU_PLAN_RESPOND;
    }

    /* Target length: roughly match message length, cap at 300 */
    plan->target_length = user_msg_len > 0 ? user_msg_len : 80;
    if (plan->target_length > TARGET_LENGTH_CAP)
        plan->target_length = TARGET_LENGTH_CAP;

    /* Ask follow-up if conversation is deep */
    plan->should_ask_question = (history_len > DEEP_CONVERSATION_THRESHOLD);

    /* Tone guidance from emotional context */
    if (emotional_context && emotional_len > 0) {
        plan->tone_guidance = hu_strndup(alloc, emotional_context, emotional_len);
        if (plan->tone_guidance)
            plan->tone_guidance_len = emotional_len;
    }

    return HU_OK;
}

hu_error_t hu_plan_build_prompt(const hu_conversation_plan_t *plan, hu_allocator_t *alloc,
                                char **out, size_t *out_len) {
    if (!plan || !alloc || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;

    *out = NULL;
    *out_len = 0;

    char buf[1024];
    size_t pos = 0;
    const size_t cap = sizeof(buf);

    pos += (size_t)snprintf(buf + pos, cap - pos,
                            "### Response Plan\nIntent: %s\nTone: %s\nTarget length: ~%zu chars\n",
                            intent_name(plan->primary_intent),
                            plan->tone_guidance && plan->tone_guidance_len > 0 ? plan->tone_guidance
                                                                               : "match context",
                            plan->target_length);
    if (pos >= cap)
        pos = cap - 1;

    if (plan->should_ask_question) {
        pos += (size_t)snprintf(buf + pos, cap - pos, "Ask a follow-up question.\n");
        if (pos >= cap)
            pos = cap - 1;
    }
    if (plan->should_share_personal) {
        pos += (size_t)snprintf(buf + pos, cap - pos,
                                "Share something personal if relevant.\n");
        if (pos >= cap)
            pos = cap - 1;
    }

    *out = hu_strndup(alloc, buf, pos);
    if (!*out)
        return HU_ERR_OUT_OF_MEMORY;
    *out_len = pos;
    return HU_OK;
}

void hu_conversation_plan_deinit(hu_conversation_plan_t *plan, hu_allocator_t *alloc) {
    if (!plan || !alloc)
        return;
    if (plan->reasoning) {
        hu_str_free(alloc, plan->reasoning);
        plan->reasoning = NULL;
    }
    plan->reasoning_len = 0;
    if (plan->tone_guidance) {
        hu_str_free(alloc, plan->tone_guidance);
        plan->tone_guidance = NULL;
    }
    plan->tone_guidance_len = 0;
}
