#include "human/cognition/dual_process.h"
#include "human/core/string.h"
#include "human/data/loader.h"
#include "human/core/json.h"

#include <ctype.h>
#include <string.h>

/* Default fallback arrays (NULL-terminated) */
static const char *DEFAULT_GREETINGS[] = {
    "hi", "hey", "hello", "yo", "sup", "morning",
    "good morning", "good evening", "good night",
    "thanks", "thank you", "ok", "okay", "cool",
    "bye", "goodbye", "see ya", "lol", "haha",
    "yes", "no", "yep", "nope", "sure", "got it",
    NULL
};

static const char *DEFAULT_QUESTION_WORDS[] = {
    "how ", "what ", "why ", "where ", "when ", "which ",
    "can you", "could you", "would you", "please ",
    "explain", "describe", "help me", "analyze",
    NULL
};

static const char *DEFAULT_COMPLEXITY_WORDS[] = {
    "implement", "architect", "design", "refactor",
    "compare", "evaluate", "trade-off", "pros and cons",
    "step by step", "plan", "strategy", "debug",
    "investigate", "research", "analyze in depth",
    NULL
};

/* Runtime loaded patterns */
static const char **s_greetings = DEFAULT_GREETINGS;
static const char **s_question_words = DEFAULT_QUESTION_WORDS;
static const char **s_complexity_words = DEFAULT_COMPLEXITY_WORDS;

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

hu_error_t hu_dual_process_data_init(hu_allocator_t *alloc) {
    if (!alloc)
        return HU_ERR_INVALID_ARGUMENT;

    char *json_data = NULL;
    size_t json_len = 0;
    hu_error_t err = hu_data_load(alloc, "cognition/dual_process_words.json", &json_data, &json_len);
    if (err != HU_OK)
        return HU_OK; /* Fail gracefully, keep defaults */

    hu_json_value_t *root = NULL;
    err = hu_json_parse(alloc, json_data, json_len, &root);
    alloc->free(alloc->ctx, json_data, json_len);
    if (err != HU_OK || !root)
        return HU_OK; /* Fail gracefully, keep defaults */

    load_patterns(alloc, root, "greetings", &s_greetings);
    load_patterns(alloc, root, "question_words", &s_question_words);
    load_patterns(alloc, root, "complexity_words", &s_complexity_words);

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

void hu_dual_process_data_cleanup(hu_allocator_t *alloc) {
    if (!alloc)
        return;

    free_patterns(alloc, s_greetings, DEFAULT_GREETINGS);
    free_patterns(alloc, s_question_words, DEFAULT_QUESTION_WORDS);
    free_patterns(alloc, s_complexity_words, DEFAULT_COMPLEXITY_WORDS);

    s_greetings = DEFAULT_GREETINGS;
    s_question_words = DEFAULT_QUESTION_WORDS;
    s_complexity_words = DEFAULT_COMPLEXITY_WORDS;
}

/* Greeting/trivial patterns that suggest System 1 */
static bool is_greeting_or_trivial(const char *msg, size_t len) {
    if (len > 80) return false;

    char lower[81];
    size_t n = len < 80 ? len : 80;
    for (size_t i = 0; i < n; i++)
        lower[i] = (char)tolower((unsigned char)msg[i]);
    lower[n] = '\0';

    /* Strip leading whitespace and punctuation for matching */
    const char *trimmed = lower;
    while (*trimmed && (isspace((unsigned char)*trimmed) || ispunct((unsigned char)*trimmed)))
        trimmed++;

    for (size_t i = 0; s_greetings[i]; i++) {
        size_t glen = strlen(s_greetings[i]);
        if (n - (size_t)(trimmed - lower) >= glen &&
            strncmp(trimmed, s_greetings[i], glen) == 0) {
            char next = trimmed[glen];
            if (next == '\0' || isspace((unsigned char)next) ||
                ispunct((unsigned char)next))
                return true;
        }
    }
    return false;
}

static bool has_question_indicators(const char *msg, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (msg[i] == '?') return true;
    }

    char lower[256];
    size_t n = len < 255 ? len : 255;
    for (size_t i = 0; i < n; i++)
        lower[i] = (char)tolower((unsigned char)msg[i]);
    lower[n] = '\0';

    for (size_t i = 0; s_question_words[i]; i++) {
        if (strstr(lower, s_question_words[i])) return true;
    }
    return false;
}

static bool has_complexity_indicators(const char *msg, size_t len) {
    char lower[512];
    size_t n = len < 511 ? len : 511;
    for (size_t i = 0; i < n; i++)
        lower[i] = (char)tolower((unsigned char)msg[i]);
    lower[n] = '\0';

    for (size_t i = 0; s_complexity_words[i]; i++) {
        if (strstr(lower, s_complexity_words[i])) return true;
    }
    return false;
}

hu_cognition_mode_t hu_cognition_dispatch(const hu_cognition_dispatch_input_t *input) {
    if (!input || !input->message || input->message_len == 0)
        return HU_COGNITION_FAST;

    float emotional_intensity = 0.0f;
    bool concerning = false;
    if (input->emotional) {
        emotional_intensity = input->emotional->state.intensity;
        concerning = input->emotional->state.concerning;
    }

    /* Emotional mode: high emotion + concerning */
    if (emotional_intensity > 0.6f || concerning)
        return HU_COGNITION_EMOTIONAL;

    /* Slow mode: recent tool usage suggests ongoing complex task */
    if (input->recent_tool_calls > 0)
        return HU_COGNITION_SLOW;

    /* Fast mode: short greetings, trivial responses */
    if (is_greeting_or_trivial(input->message, input->message_len) &&
        emotional_intensity < 0.3f)
        return HU_COGNITION_FAST;

    /* Fast mode: very short, no question marks, no complexity */
    if (input->message_len < 30 &&
        !has_question_indicators(input->message, input->message_len) &&
        emotional_intensity < 0.3f)
        return HU_COGNITION_FAST;

    /* Slow mode: long messages, questions, complexity indicators */
    if (input->message_len > 200 ||
        has_complexity_indicators(input->message, input->message_len))
        return HU_COGNITION_SLOW;

    /* Default: questions get slow, statements get fast */
    if (has_question_indicators(input->message, input->message_len))
        return HU_COGNITION_SLOW;

    return HU_COGNITION_FAST;
}

hu_cognition_budget_t hu_cognition_get_budget(hu_cognition_mode_t mode,
                                               uint32_t agent_max_iters) {
    hu_cognition_budget_t budget;
    memset(&budget, 0, sizeof(budget));
    budget.mode = mode;

    switch (mode) {
    case HU_COGNITION_FAST:
        budget.max_memory_entries = 3;
        budget.max_memory_chars = 1500;
        budget.max_tool_iterations = 2;
        budget.enable_planning = false;
        budget.enable_tree_of_thought = false;
        budget.enable_mid_turn_retrieval = false;
        budget.enable_reflection = false;
        budget.prioritize_empathy = false;
        break;

    case HU_COGNITION_SLOW:
        budget.max_memory_entries = 10;
        budget.max_memory_chars = 4000;
        budget.max_tool_iterations = agent_max_iters;
        budget.enable_planning = true;
        budget.enable_tree_of_thought = true;
        budget.enable_mid_turn_retrieval = true;
        budget.enable_reflection = true;
        budget.prioritize_empathy = false;
        break;

    case HU_COGNITION_EMOTIONAL:
        budget.max_memory_entries = 10;
        budget.max_memory_chars = 4000;
        budget.max_tool_iterations = agent_max_iters;
        budget.enable_planning = false;
        budget.enable_tree_of_thought = false;
        budget.enable_mid_turn_retrieval = true;
        budget.enable_reflection = true;
        budget.prioritize_empathy = true;
        break;
    }

    return budget;
}

const char *hu_cognition_mode_name(hu_cognition_mode_t mode) {
    switch (mode) {
    case HU_COGNITION_FAST:      return "fast";
    case HU_COGNITION_SLOW:      return "slow";
    case HU_COGNITION_EMOTIONAL: return "emotional";
    default:                     return "unknown";
    }
}
