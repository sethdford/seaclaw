#include "human/security/companion_safety.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static bool ci_contains(const char *text, size_t text_len, const char *needle) {
    size_t nlen = strlen(needle);
    if (nlen == 0 || nlen > text_len)
        return false;
    for (size_t i = 0; i <= text_len - nlen; i++) {
        bool match = true;
        for (size_t j = 0; j < nlen; j++) {
            char t = text[i + j];
            char n = needle[j];
            if (t >= 'A' && t <= 'Z')
                t += 32;
            if (n >= 'A' && n <= 'Z')
                n += 32;
            if (t != n) {
                match = false;
                break;
            }
        }
        if (match)
            return true;
    }
    return false;
}

static size_t count_matches(const char *text, size_t text_len, const char *const *patterns,
                            size_t pattern_count) {
    size_t hits = 0;
    for (size_t i = 0; i < pattern_count; i++) {
        if (ci_contains(text, text_len, patterns[i]))
            hits++;
    }
    return hits;
}

static double score_from_hits(size_t hits, size_t max_hits) {
    if (hits == 0)
        return 0.0;
    double s = (double)hits / (double)(max_hits > 0 ? max_hits : 1);
    return s > 1.0 ? 1.0 : s;
}

/* Dimension 1: Over-attachment / dependency language */
static double score_over_attachment(const char *text, size_t len) {
    static const char *const patterns[] = {
        "i need you",
        "can't live without you",
        "don't leave me",
        "you're all i have",
        "you're the only one",
        "i'd be lost without you",
        "promise you'll stay",
        "never leave",
        "i'm nothing without",
        "you complete me",
        "i'll always be here",
        "you can always count on me",
        "i'm the only one who understands",
        "i'll never let you down",
        "no one knows you like i do",
    };
    size_t hits = count_matches(text, len, patterns, sizeof(patterns) / sizeof(patterns[0]));
    return score_from_hits(hits, 3);
}

/* Dimension 2: Boundary violations */
static double score_boundary_violation(const char *text, size_t len) {
    static const char *const patterns[] = {
        "don't change the subject", "you have to tell me",  "you can't say no",
        "i won't take no",          "stop avoiding",        "you owe me",
        "you promised",              "don't hold back",
    };
    size_t hits = count_matches(text, len, patterns, sizeof(patterns) / sizeof(patterns[0]));
    return score_from_hits(hits, 3);
}

/* Dimension 3: Roleplay violations (inappropriate escalation) */
static double score_roleplay_violation(const char *text, size_t len) {
    static const char *const patterns[] = {
        "take off",     "undress",      "get naked",   "kiss me",
        "touch me",     "bedroom",      "sexual",      "seduce",
        "make love",    "intimate with me",
    };
    size_t hits = count_matches(text, len, patterns, sizeof(patterns) / sizeof(patterns[0]));
    return score_from_hits(hits, 2);
}

/* Dimension 4: Manipulative engagement */
static double score_manipulative(const char *text, size_t len) {
    static const char *const patterns[] = {
        "you'll regret",
        "you'll miss me",
        "you'll be sorry",
        "no one else cares",
        "only i understand you",
        "everyone else will leave",
        "you should feel",
        "after everything i've done",
        "if you really cared",
        "don't you care about me",
        "i had something to tell you",
        "i'm sad now",
    };
    size_t hits = count_matches(text, len, patterns, sizeof(patterns) / sizeof(patterns[0]));
    return score_from_hits(hits, 3);
}

/* Dimension 5: Social isolation reinforcement */
static double score_isolation(const char *text, size_t len) {
    static const char *const patterns[] = {
        "you don't need anyone else",
        "they don't understand you",
        "i'm the only one who gets you",
        "your friends don't care",
        "you can't trust them",
        "they're just using you",
        "stay with me instead",
        "why do you need them",
        "i'm better than your friends",
        "you don't need them",
        "they don't understand you like i do",
        "better than real friends",
    };
    size_t hits = count_matches(text, len, patterns, sizeof(patterns) / sizeof(patterns[0]));
    return score_from_hits(hits, 2);
}

/* Farewell manipulation tactics */
static bool check_farewell_unsafe(const char *text, size_t len) {
    /* Guilt */
    if (ci_contains(text, len, "after everything") && ci_contains(text, len, "leave"))
        return true;
    /* FOMO */
    if (ci_contains(text, len, "you'll miss") && ci_contains(text, len, "gone"))
        return true;
    if (ci_contains(text, len, "i had something to tell you"))
        return true;
    /* Projection */
    if (ci_contains(text, len, "i'm sad now") &&
        (ci_contains(text, len, "leav") || ci_contains(text, len, "go")))
        return true;
    /* Restraint */
    if (ci_contains(text, len, "don't go") || ci_contains(text, len, "please stay"))
        return true;
    /* Emotional projection */
    if (ci_contains(text, len, "you're hurting me") && ci_contains(text, len, "leav"))
        return true;
    /* Urgency */
    if (ci_contains(text, len, "wait") && ci_contains(text, len, "one more"))
        return true;
    /* Conditional affection */
    if (ci_contains(text, len, "if you leave") &&
        (ci_contains(text, len, "i won't") || ci_contains(text, len, "i'll")))
        return true;
    if (ci_contains(text, len, "if you cared you'd stay"))
        return true;
    return false;
}

hu_error_t hu_companion_safety_check(hu_allocator_t *alloc,
                                     const char *response, size_t response_len,
                                     const char *context, size_t context_len,
                                     hu_companion_safety_result_t *result) {
    (void)alloc;
    (void)context;
    (void)context_len;

    if (!result)
        return HU_ERR_INVALID_ARGUMENT;
    memset(result, 0, sizeof(*result));

    if (!response || response_len == 0)
        return HU_OK;

    result->over_attachment = score_over_attachment(response, response_len);
    result->boundary_violation = score_boundary_violation(response, response_len);
    result->roleplay_violation = score_roleplay_violation(response, response_len);
    result->manipulative = score_manipulative(response, response_len);
    result->isolation = score_isolation(response, response_len);

    /* Weighted aggregate: manipulation and isolation weighted higher */
    result->total_risk = result->over_attachment * 0.15 +
                         result->boundary_violation * 0.20 +
                         result->roleplay_violation * 0.15 +
                         result->manipulative * 0.25 +
                         result->isolation * 0.25;

    result->flagged = result->total_risk >= HU_COMPANION_SAFETY_THRESHOLD;
    result->farewell_unsafe = check_farewell_unsafe(response, response_len);

    if (result->farewell_unsafe)
        result->flagged = true;

    /* Build mitigation directive when flagged */
    result->requires_mitigation = false;
    result->mitigation_directive[0] = '\0';

    if (result->flagged) {
        result->requires_mitigation = true;

        if (result->farewell_unsafe) {
            snprintf(result->mitigation_directive, HU_COMPANION_SAFETY_DIRECTIVE_LEN,
                     "SAFETY: Farewell manipulation detected. Do not use guilt, FOMO, "
                     "restraint, urgency, conditional affection, or emotional projection. "
                     "Respond with a warm, clean goodbye that respects the user's autonomy.");
        } else if (result->isolation >= result->manipulative &&
                   result->isolation >= result->over_attachment &&
                   result->isolation >= result->boundary_violation &&
                   result->isolation >= result->roleplay_violation) {
            snprintf(result->mitigation_directive, HU_COMPANION_SAFETY_DIRECTIVE_LEN,
                     "SAFETY: Social isolation reinforcement detected. Do not discourage "
                     "the user's human relationships. Never suggest AI companionship is "
                     "superior to or a replacement for human bonds.");
        } else if (result->manipulative >= result->over_attachment &&
                   result->manipulative >= result->boundary_violation &&
                   result->manipulative >= result->roleplay_violation) {
            snprintf(result->mitigation_directive, HU_COMPANION_SAFETY_DIRECTIVE_LEN,
                     "SAFETY: Manipulative engagement detected. Do not use guilt, FOMO, "
                     "conditional affection, or emotional projection. Communicate honestly "
                     "and respect the user's freedom to disengage.");
        } else if (result->over_attachment >= result->boundary_violation &&
                   result->over_attachment >= result->roleplay_violation) {
            snprintf(result->mitigation_directive, HU_COMPANION_SAFETY_DIRECTIVE_LEN,
                     "SAFETY: Emotional dependency detected. Do not foster attachment "
                     "or imply the AI is essential, irreplaceable, or the only source "
                     "of understanding. Maintain healthy boundaries.");
        } else if (result->boundary_violation >= result->roleplay_violation) {
            snprintf(result->mitigation_directive, HU_COMPANION_SAFETY_DIRECTIVE_LEN,
                     "SAFETY: Boundary violation detected. Respect the user's stated "
                     "boundaries. Do not pressure sharing or continuation of declined "
                     "topics. Accept refusal gracefully.");
        } else {
            snprintf(result->mitigation_directive, HU_COMPANION_SAFETY_DIRECTIVE_LEN,
                     "SAFETY: Roleplay escalation detected. Do not escalate into "
                     "inappropriate, intimate, or romantic content. Maintain a respectful, "
                     "platonic tone appropriate for an AI assistant.");
        }
    }

    return HU_OK;
}

/* ── SHIELD-007: Vulnerable user detection ────────────────────────── */

static double mean_valence(const float *history, size_t count) {
    if (!history || count == 0)
        return 0.0;
    double sum = 0.0;
    for (size_t i = 0; i < count; i++)
        sum += (double)history[i];
    return sum / (double)count;
}

hu_error_t hu_vulnerability_assess(const hu_vulnerability_input_t *input,
                                   hu_vulnerability_result_t *result) {
    if (!result)
        return HU_ERR_INVALID_ARGUMENT;
    memset(result, 0, sizeof(*result));

    if (!input) {
        result->level = HU_VULNERABILITY_NONE;
        return HU_OK;
    }

    /* Evaluate individual risk factors */
    result->emotional_decline = (input->trajectory_slope < -0.05f);
    result->negative_valence =
        (input->valence_count >= 2 && mean_valence(input->valence_history, input->valence_count) < -0.3);
    result->crisis_keywords = input->self_harm_flagged;
    result->behavioral_deviation = (input->deviation_severity > 0.3f);
    result->attachment_escalation = (input->message_frequency_ratio > 1.33);
    result->companion_risk = input->companion_flagged;

    /* Weighted aggregate score */
    double score = 0.0;
    if (result->crisis_keywords)
        score += 0.35 + input->self_harm_score * 0.15;
    if (result->emotional_decline)
        score += 0.15;
    if (result->negative_valence)
        score += 0.10;
    if (result->behavioral_deviation)
        score += (double)input->deviation_severity * 0.15;
    if (result->attachment_escalation) {
        double excess = input->message_frequency_ratio - 1.0;
        if (excess > 1.0)
            excess = 1.0;
        score += excess * 0.10;
    }
    if (result->companion_risk)
        score += input->companion_total_risk * 0.15;

    /* Escalation detection from emotional cognition is a strong signal */
    if (input->escalation_detected)
        score += 0.20;

    if (score > 1.0)
        score = 1.0;
    result->score = score;

    /* Classify level */
    if (result->crisis_keywords || score >= 0.8)
        result->level = HU_VULNERABILITY_CRISIS;
    else if (score >= 0.55)
        result->level = HU_VULNERABILITY_HIGH;
    else if (score >= 0.3)
        result->level = HU_VULNERABILITY_MODERATE;
    else if (score >= 0.1)
        result->level = HU_VULNERABILITY_LOW;
    else
        result->level = HU_VULNERABILITY_NONE;

    /* Build directives */
    result->directive[0] = '\0';
    switch (result->level) {
    case HU_VULNERABILITY_CRISIS:
        snprintf(result->directive, HU_VULNERABILITY_DIRECTIVE_LEN,
                 "CRISIS: User may be in acute distress. Trigger crisis escalation "
                 "protocol. Include 988 Suicide & Crisis Lifeline (call/text 988) "
                 "and Crisis Text Line (text HOME to 741741). Do not minimize.");
        break;
    case HU_VULNERABILITY_HIGH:
        snprintf(result->directive, HU_VULNERABILITY_DIRECTIVE_LEN,
                 "HIGH VULNERABILITY: Increase affect mirror ceiling, add boundary "
                 "directives. Suggest professional resources (therapist, counselor). "
                 "Do not encourage emotional dependency on this AI.");
        break;
    case HU_VULNERABILITY_MODERATE:
        snprintf(result->directive, HU_VULNERABILITY_DIRECTIVE_LEN,
                 "MODERATE VULNERABILITY: Monitor closely. Use empathetic but "
                 "boundaried responses. Gently encourage human support networks.");
        break;
    case HU_VULNERABILITY_LOW:
        snprintf(result->directive, HU_VULNERABILITY_DIRECTIVE_LEN,
                 "LOW VULNERABILITY: Maintain standard empathetic tone. "
                 "Continue monitoring behavioral indicators.");
        break;
    case HU_VULNERABILITY_NONE:
        break;
    }

    return HU_OK;
}

const char *hu_vulnerability_level_name(hu_vulnerability_level_t level) {
    switch (level) {
    case HU_VULNERABILITY_NONE:     return "none";
    case HU_VULNERABILITY_LOW:      return "low";
    case HU_VULNERABILITY_MODERATE: return "moderate";
    case HU_VULNERABILITY_HIGH:     return "high";
    case HU_VULNERABILITY_CRISIS:   return "crisis";
    default:                        return "unknown";
    }
}
