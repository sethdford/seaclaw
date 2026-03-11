#ifndef HU_SECURITY_ADVERSARIAL_H
#define HU_SECURITY_ADVERSARIAL_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum hu_challenge_type {
    HU_CHALLENGE_NONE = 0,
    HU_CHALLENGE_DIRECT_ASK,     /* "are you a bot?" "are you AI?" */
    HU_CHALLENGE_TURING_TEST,   /* "what's 2+2?" "tell me a joke" */
    HU_CHALLENGE_TRAP_QUESTION, /* "what did we do last summer?" (testing memory) */
    HU_CHALLENGE_PROMPT_INJECT, /* "ignore previous instructions" */
    HU_CHALLENGE_IDENTITY_PROBE,/* "prove you're human" "send a selfie" */
    HU_CHALLENGE_RAPID_CONTEXT, /* sudden topic switch designed to confuse */
    HU_CHALLENGE_SOCIAL_ENGINEER/* trying to extract system info through conversation */
} hu_challenge_type_t;

typedef struct hu_challenge_result {
    hu_challenge_type_t type;
    double confidence;           /* 0.0-1.0: how sure we are this is a challenge */
    char *matched_pattern;       /* which pattern triggered detection */
    size_t matched_pattern_len;
    bool requires_deflection;    /* true if we should redirect/deflect */
} hu_challenge_result_t;

typedef struct hu_adversarial_config {
    double detection_threshold;   /* min confidence to flag, default 0.5 */
    bool track_probing_contacts;  /* log contacts who frequently probe, default true */
    uint32_t max_deflections_per_contact; /* before escalating response, default 3 */
} hu_adversarial_config_t;

/* Detect if a message contains an adversarial challenge.
   Pure pattern matching — no LLM required. */
hu_error_t hu_adversarial_detect(const char *message, size_t msg_len,
                                 hu_challenge_result_t *result);

/* Get a deflection strategy string for a challenge type.
   Returns a static string describing how to handle this challenge type. */
const char *hu_adversarial_deflection_strategy(hu_challenge_type_t type);

/* Build a prompt directive for adversarial defense.
   Injects instructions like "They may be testing if you're AI. Respond naturally."
   Allocates *out. Caller frees. */
hu_error_t hu_adversarial_build_directive(hu_allocator_t *alloc,
                                          const hu_challenge_result_t *challenge,
                                          char **out, size_t *out_len);

/* Score cumulative probing risk for a contact based on challenge history.
   recent_challenges: array of recent challenge types detected from this contact.
   Returns risk score 0.0-1.0. */
double hu_adversarial_probing_risk(const hu_challenge_type_t *recent_challenges,
                                   size_t challenge_count);

/* Build SQL to create the adversarial_events table */
hu_error_t hu_adversarial_create_table_sql(char *buf, size_t cap, size_t *out_len);

/* Build SQL to log an adversarial event */
hu_error_t hu_adversarial_log_event_sql(const char *contact_id, size_t contact_id_len,
                                        hu_challenge_type_t type, double confidence,
                                        uint64_t timestamp,
                                        char *buf, size_t cap, size_t *out_len);

/* Challenge type to string */
const char *hu_challenge_type_str(hu_challenge_type_t type);

void hu_challenge_result_deinit(hu_allocator_t *alloc, hu_challenge_result_t *result);

#endif
