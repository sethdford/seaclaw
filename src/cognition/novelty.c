#include "human/cognition/novelty.h"
#include "human/core/string.h"
#include <ctype.h>
#include <string.h>

void hu_novelty_tracker_init(hu_novelty_tracker_t *tracker) {
    if (!tracker)
        return;
    memset(tracker, 0, sizeof(*tracker));
    tracker->cooldown_turns = 10;
}

static uint32_t novelty_hash(const char *s, size_t len) {
    uint32_t h = 0x811c9dc5u;
    for (size_t i = 0; i < len; i++) {
        h ^= (uint32_t)(unsigned char)tolower((unsigned char)s[i]);
        h *= 0x01000193u;
    }
    return h;
}

static bool novelty_seen_hash(const hu_novelty_tracker_t *t, uint32_t h) {
    for (size_t i = 0; i < t->seen_count; i++) {
        if (t->seen_hashes[i] == h)
            return true;
    }
    return false;
}

static void novelty_record_hash(hu_novelty_tracker_t *t, uint32_t h) {
    if (novelty_seen_hash(t, h))
        return;
    if (t->seen_count < HU_NOVELTY_MAX_SEEN) {
        t->seen_hashes[t->seen_count++] = h;
    } else {
        memmove(t->seen_hashes, t->seen_hashes + 1,
                (HU_NOVELTY_MAX_SEEN - 1) * sizeof(uint32_t));
        t->seen_hashes[HU_NOVELTY_MAX_SEEN - 1] = h;
    }
}

static bool novelty_topic_contains(const char *const *topics, size_t count, const char *tok,
                                   size_t tok_len) {
    if (!topics || count == 0U || tok_len == 0U)
        return false;
    char buf[256];
    if (tok_len >= sizeof buf)
        return false;
    memcpy(buf, tok, tok_len);
    buf[tok_len] = '\0';

    for (size_t i = 0; i < count; i++) {
        if (!topics[i])
            continue;
        if (hu_strcasestr(topics[i], buf) != NULL)
            return true;
    }
    return false;
}

hu_error_t hu_novelty_evaluate(hu_allocator_t *alloc, hu_novelty_tracker_t *tracker,
                               const char *message, size_t message_len, const char *const *known_topics,
                               size_t known_count, const char *const *stm_topics, size_t stm_count,
                               hu_novelty_signal_t *out) {
    if (!alloc || !tracker || !out)
        return HU_ERR_INVALID_ARGUMENT;
    if (message_len > 0U && !message)
        return HU_ERR_INVALID_ARGUMENT;
    if (known_count > 0U && !known_topics)
        return HU_ERR_INVALID_ARGUMENT;
    if (stm_count > 0U && !stm_topics)
        return HU_ERR_INVALID_ARGUMENT;

    memset(out, 0, sizeof(*out));

    tracker->turns_since_last_surprise++;

    if (message_len == 0U || !message) {
        out->novelty_score = 0.f;
        return HU_OK;
    }

    size_t total_candidates = 0;
    size_t unmatched = 0;
    size_t first_unmatched_off = 0;
    size_t first_unmatched_len = 0;
    bool have_first = false;

    size_t i = 0;
    while (i < message_len) {
        while (i < message_len && isspace((unsigned char)message[i]))
            i++;
        if (i >= message_len)
            break;
        size_t w0 = i;
        while (i < message_len && !isspace((unsigned char)message[i]))
            i++;
        size_t wlen = i - w0;
        if (wlen <= 5U || !islower((unsigned char)message[w0])) {
            continue;
        }

        total_candidates++;
        uint32_t wh = novelty_hash(message + w0, wlen);
        bool known = novelty_topic_contains(known_topics, known_count, message + w0, wlen) ||
                     novelty_topic_contains(stm_topics, stm_count, message + w0, wlen) ||
                     novelty_seen_hash(tracker, wh);
        novelty_record_hash(tracker, wh);
        if (!known) {
            unmatched++;
            if (!have_first) {
                first_unmatched_off = w0;
                first_unmatched_len = wlen;
                have_first = true;
            }
        }
    }

    if (total_candidates == 0U) {
        out->novelty_score = 0.f;
    } else {
        out->novelty_score = (float)unmatched / (float)total_candidates;
    }

    if (out->novelty_score > 0.7f && tracker->turns_since_last_surprise >= tracker->cooldown_turns &&
        have_first) {
        char *elem = hu_strndup(alloc, message + first_unmatched_off, first_unmatched_len);
        if (!elem)
            return HU_ERR_OUT_OF_MEMORY;

        char *prompt =
            hu_sprintf(alloc,
                       "[NOVELTY: The user mentioned \"%s\" — this is genuinely new to you. Express "
                       "authentic curiosity, not performative surprise. Ask a real question.]",
                       elem);
        if (!prompt) {
            hu_str_free(alloc, elem);
            return HU_ERR_OUT_OF_MEMORY;
        }

        out->novel_element = elem;
        out->surprise_prompt = prompt;
        tracker->turns_since_last_surprise = 0;
    }

    return HU_OK;
}

void hu_novelty_signal_free(hu_allocator_t *alloc, hu_novelty_signal_t *signal) {
    if (!alloc || !signal)
        return;
    hu_str_free(alloc, signal->novel_element);
    hu_str_free(alloc, signal->surprise_prompt);
    signal->novel_element = NULL;
    signal->surprise_prompt = NULL;
}
