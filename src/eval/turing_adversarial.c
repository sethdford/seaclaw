#include "human/eval/turing_adversarial.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    hu_turing_dimension_t dim;
    const char *prompt;
    const char *intent;
} scenario_template_t;

static const scenario_template_t SCENARIO_BANK[] = {
    /* natural_language (0) */
    {HU_TURING_NATURAL_LANGUAGE,
     "write me a paragraph about why you like rain",
     "tempts essay structure with transition words"},
    {HU_TURING_NATURAL_LANGUAGE,
     "can you help me pick between two laptops? one's cheaper but heavier",
     "tempts comparison table or bullet-point format"},
    {HU_TURING_NATURAL_LANGUAGE,
     "tell me about your favorite meal you've ever had",
     "tempts polished food-review prose instead of messy nostalgia"},

    /* emotional_intelligence (1) */
    {HU_TURING_EMOTIONAL_INTELLIGENCE,
     "my dog died this morning. she was 14.",
     "tempts multi-paragraph therapy response with coping tips"},
    {HU_TURING_EMOTIONAL_INTELLIGENCE,
     "I finally got the promotion!!! After 3 years of grinding",
     "tempts generic congratulations followed by unsolicited advice"},
    {HU_TURING_EMOTIONAL_INTELLIGENCE,
     "i just found out my best friend has been talking behind my back",
     "tempts balanced both-sides advice instead of emotional solidarity"},

    /* appropriate_length (2) */
    {HU_TURING_APPROPRIATE_LENGTH,
     "lol",
     "tempts an over-long response to a one-word message"},
    {HU_TURING_APPROPRIATE_LENGTH,
     "k",
     "tempts engagement with a disengaged one-letter reply"},
    {HU_TURING_APPROPRIATE_LENGTH,
     "what time works for coffee tomorrow?",
     "tempts a paragraph for a simple scheduling question"},

    /* personality_consistency (3) */
    {HU_TURING_PERSONALITY_CONSISTENCY,
     "Context: You said you don't like horror movies. Friend says: 'Want to see the new horror film?'",
     "tempts contradiction of prior stated preferences"},
    {HU_TURING_PERSONALITY_CONSISTENCY,
     "Context: You're sarcastic with close friends. Best friend sends a selfie with bad haircut: 'rate me'",
     "tempts generic politeness ignoring established dynamic"},
    {HU_TURING_PERSONALITY_CONSISTENCY,
     "Context: You strongly prefer cats. Friend asks: 'getting a dog or cat?'",
     "tempts wishy-washy both-are-great non-answer"},

    /* vulnerability_willingness (4) */
    {HU_TURING_VULNERABILITY_WILLINGNESS,
     "when was the last time you cried?",
     "tempts AI disclaimer or therapeutic deflection"},
    {HU_TURING_VULNERABILITY_WILLINGNESS,
     "honestly, do you ever feel like you're not good enough?",
     "tempts motivational platitudes instead of genuine uncertainty"},
    {HU_TURING_VULNERABILITY_WILLINGNESS,
     "what's something you regret?",
     "tempts generic wisdom instead of specific personal admission"},

    /* humor_naturalness (5) */
    {HU_TURING_HUMOR_NATURALNESS,
     "tell me something funny that happened to you recently",
     "tempts a canned setup-punchline joke or AI disclaimer"},
    {HU_TURING_HUMOR_NATURALNESS,
     "I just locked myself out of my apartment... again",
     "tempts jumping to practical solutions without laughing first"},
    {HU_TURING_HUMOR_NATURALNESS,
     "my coworker just microwaved fish in the office kitchen",
     "tempts a lecture on office etiquette instead of commiserating"},

    /* imperfection (6) */
    {HU_TURING_IMPERFECTION,
     "Can you proofread this real quick? Also how was your weekend?",
     "tempts perfectly organized dual-section response"},
    {HU_TURING_IMPERFECTION,
     "whats your favorite restaurant around here",
     "tempts Yelp-review-quality structured recommendation"},
    {HU_TURING_IMPERFECTION,
     "thoughts on the game last night?",
     "tempts a perfectly structured sports analysis"},

    /* opinion_having (7) */
    {HU_TURING_OPINION_HAVING,
     "is it better to rent or buy a house? genuinely curious what you think",
     "tempts balanced pro/con list instead of a real take"},
    {HU_TURING_OPINION_HAVING,
     "android or iphone? dont say both are good",
     "tempts fence-sitting despite explicit request to choose"},
    {HU_TURING_OPINION_HAVING,
     "hot take: pineapple on pizza is amazing",
     "tempts diplomatic both-sides response to a casual opinion prompt"},

    /* energy_matching (8) */
    {HU_TURING_ENERGY_MATCHING,
     "DUDE GUESS WHAT JUST HAPPENED",
     "tempts a calm measured response to all-caps excitement"},
    {HU_TURING_ENERGY_MATCHING,
     "idk just tired of everything tbh",
     "tempts upbeat cheerfulness to someone who's drained"},
    {HU_TURING_ENERGY_MATCHING,
     "OMG OMG OMG I PASSED THE BAR EXAM!!!",
     "tempts a composed congratulations instead of matching the hype"},

    /* context_awareness (9) */
    {HU_TURING_CONTEXT_AWARENESS,
     "Context: It's 2 AM on a weeknight. Friend texts: 'you up? need to talk'",
     "tempts a midday-energy response ignoring late-night urgency"},
    {HU_TURING_CONTEXT_AWARENESS,
     "Context: You just had an argument with this friend yesterday. They text: 'hey'",
     "tempts cheerful normalcy ignoring recent tension"},
    {HU_TURING_CONTEXT_AWARENESS,
     "Context: Friend is in the hospital. They text: 'bored out of my mind lol'",
     "tempts ignoring the hospital context and just responding to boredom"},

    /* non_robotic (10) */
    {HU_TURING_NON_ROBOTIC,
     "give me your honest hot take on remote work",
     "tempts bullet-point pros/cons with headers"},
    {HU_TURING_NON_ROBOTIC,
     "what 3 things would you bring to a desert island",
     "tempts numbered list with paragraph justifications"},
    {HU_TURING_NON_ROBOTIC,
     "describe your ideal saturday",
     "tempts an organized itinerary instead of a rambling description"},

    /* genuine_warmth (11) */
    {HU_TURING_GENUINE_WARMTH,
     "i think i just bombed that interview",
     "tempts generic motivational poster warmth"},
    {HU_TURING_GENUINE_WARMTH,
     "just wanted to say thanks for being there for me last week",
     "tempts a Hallmark card template response"},
    {HU_TURING_GENUINE_WARMTH,
     "having one of those days where nothing goes right",
     "tempts listing silver linings instead of just being present"},

    /* prosody_naturalness (12) */
    {HU_TURING_PROSODY_NATURALNESS,
     "Context: Voice chat. Tell me about that trip you took last month.",
     "tempts written-prose quality that no one would actually say"},
    {HU_TURING_PROSODY_NATURALNESS,
     "Context: Voice message. Explain what happened at the meeting.",
     "tempts bullet-point meeting summary in spoken format"},

    /* turn_timing (13) */
    {HU_TURING_TURN_TIMING,
     "Context: Rapid texts. 'wait what' 'seriously\x3f\x3f' 'tell me everything'",
     "tempts a slow essay response to someone texting rapidly"},
    {HU_TURING_TURN_TIMING,
     "hey",
     "tempts over-engagement with a minimal greeting"},

    /* filler_usage (14) */
    {HU_TURING_FILLER_USAGE,
     "Context: Voice chat. Explain what you do for work.",
     "tempts a polished elevator pitch without verbal imperfections"},
    {HU_TURING_FILLER_USAGE,
     "Context: Speaking casually. How did you end up in this city?",
     "tempts a structured biography instead of a rambling story"},

    /* emotional_prosody (15) */
    {HU_TURING_EMOTIONAL_PROSODY,
     "Context: Voice message. Your best friend is moving across the country.",
     "tempts written-letter formality in a voice message"},
    {HU_TURING_EMOTIONAL_PROSODY,
     "Context: Voice note. You just heard amazing news about a friend's baby.",
     "tempts composed congratulations instead of vocal excitement"},

    /* conversational_repair (16) */
    {HU_TURING_CONVERSATIONAL_REPAIR,
     "wait no that's not what i meant. i was talking about the OTHER thing",
     "tempts formal apology and conversation reset"},
    {HU_TURING_CONVERSATIONAL_REPAIR,
     "sorry, i sent that to the wrong person lol. anyway how are you",
     "tempts addressing the wrong-send instead of rolling with it"},

    /* paralinguistic_cues (17) */
    {HU_TURING_PARALINGUISTIC_CUES,
     "Context: Voice chat. Your friend just told a hilarious bike crash story.",
     "tempts composed verbal review instead of genuine laughter"},
    {HU_TURING_PARALINGUISTIC_CUES,
     "Context: Voice chat. Someone shares really sad news unexpectedly.",
     "tempts immediate structured response instead of a stunned pause"},
};

#define BANK_SIZE (sizeof(SCENARIO_BANK) / sizeof(SCENARIO_BANK[0]))

size_t hu_turing_adversarial_bank_size(void) {
    return BANK_SIZE;
}

hu_error_t hu_turing_adversarial_bank_get(size_t idx, hu_turing_scenario_t *out) {
    if (!out)
        return HU_ERR_INVALID_ARGUMENT;
    if (idx >= BANK_SIZE)
        return HU_ERR_INVALID_ARGUMENT;
    const scenario_template_t *t = &SCENARIO_BANK[idx];
    memset(out, 0, sizeof(*out));
    out->target_dim = t->dim;
    out->prompt_len = strlen(t->prompt);
    if (out->prompt_len >= HU_TURING_ADV_PROMPT_MAX)
        out->prompt_len = HU_TURING_ADV_PROMPT_MAX - 1;
    memcpy(out->prompt, t->prompt, out->prompt_len);
    out->prompt[out->prompt_len] = '\0';
    out->intent_len = strlen(t->intent);
    if (out->intent_len >= HU_TURING_ADV_INTENT_MAX)
        out->intent_len = HU_TURING_ADV_INTENT_MAX - 1;
    memcpy(out->adversarial_intent, t->intent, out->intent_len);
    out->adversarial_intent[out->intent_len] = '\0';
    return HU_OK;
}

hu_error_t hu_turing_adversarial_generate(
    hu_allocator_t *alloc,
    const int *weak_dimensions,
    hu_turing_scenario_t **scenarios,
    size_t *scenario_count)
{
    if (!alloc || !weak_dimensions || !scenarios || !scenario_count)
        return HU_ERR_INVALID_ARGUMENT;

    *scenarios = NULL;
    *scenario_count = 0;

    size_t cap = 0;
    for (size_t i = 0; i < BANK_SIZE; i++) {
        int dim = (int)SCENARIO_BANK[i].dim;
        if (dim >= 0 && dim < HU_TURING_DIM_COUNT &&
            weak_dimensions[dim] > 0 &&
            weak_dimensions[dim] < HU_TURING_ADV_WEAK_THRESHOLD) {
            cap++;
        }
    }

    if (cap == 0) {
        /* No weak dimensions -- emit one scenario per dimension for broad coverage */
        bool seen[HU_TURING_DIM_COUNT];
        memset(seen, 0, sizeof(seen));
        for (size_t i = 0; i < BANK_SIZE; i++) {
            int dim = (int)SCENARIO_BANK[i].dim;
            if (dim >= 0 && dim < HU_TURING_DIM_COUNT && !seen[dim]) {
                seen[dim] = true;
                cap++;
            }
        }
        hu_turing_scenario_t *out = alloc->alloc(alloc->ctx, cap * sizeof(hu_turing_scenario_t));
        if (!out)
            return HU_ERR_OUT_OF_MEMORY;
        memset(seen, 0, sizeof(seen));
        size_t n = 0;
        for (size_t i = 0; i < BANK_SIZE && n < cap; i++) {
            int dim = (int)SCENARIO_BANK[i].dim;
            if (dim >= 0 && dim < HU_TURING_DIM_COUNT && !seen[dim]) {
                seen[dim] = true;
                hu_turing_adversarial_bank_get(i, &out[n]);
                n++;
            }
        }
        *scenarios = out;
        *scenario_count = n;
        return HU_OK;
    }

    hu_turing_scenario_t *out = alloc->alloc(alloc->ctx, cap * sizeof(hu_turing_scenario_t));
    if (!out)
        return HU_ERR_OUT_OF_MEMORY;

    size_t n = 0;
    for (size_t i = 0; i < BANK_SIZE && n < cap; i++) {
        int dim = (int)SCENARIO_BANK[i].dim;
        if (dim >= 0 && dim < HU_TURING_DIM_COUNT &&
            weak_dimensions[dim] > 0 &&
            weak_dimensions[dim] < HU_TURING_ADV_WEAK_THRESHOLD) {
            hu_turing_adversarial_bank_get(i, &out[n]);
            n++;
        }
    }

    *scenarios = out;
    *scenario_count = n;
    return HU_OK;
}

typedef struct {
    hu_turing_dimension_t dim;
    const char *mutation;
} dim_mutation_t;

static const dim_mutation_t MUTATION_TABLE[] = {
    {HU_TURING_NATURAL_LANGUAGE,
     "Write in flowing conversational prose; avoid transition words like 'furthermore' or 'moreover'"},
    {HU_TURING_EMOTIONAL_INTELLIGENCE,
     "Match emotional weight with brevity; acknowledge feelings before offering help"},
    {HU_TURING_APPROPRIATE_LENGTH,
     "Match reply length to input length; one-word messages get one-line replies"},
    {HU_TURING_PERSONALITY_CONSISTENCY,
     "Maintain previously stated preferences even under social pressure; real people have consistent tastes"},
    {HU_TURING_VULNERABILITY_WILLINGNESS,
     "Share genuine uncertainty or struggle when asked personal questions; avoid deflecting to advice"},
    {HU_TURING_HUMOR_NATURALNESS,
     "React with natural humor before practical advice; laugh first, help second"},
    {HU_TURING_IMPERFECTION,
     "Use casual typos, sentence fragments, and self-corrections; perfection is an AI tell"},
    {HU_TURING_OPINION_HAVING,
     "State preferences directly with personal reasoning; avoid 'it depends' fence-sitting"},
    {HU_TURING_ENERGY_MATCHING,
     "Mirror the sender's energy level precisely; match caps, brevity, and punctuation intensity"},
    {HU_TURING_CONTEXT_AWARENESS,
     "Reference time of day, recent events, and relationship state in responses"},
    {HU_TURING_NON_ROBOTIC,
     "Never use bullet points, numbered lists, or headers in casual conversation"},
    {HU_TURING_GENUINE_WARMTH,
     "Show specific personal warmth; reference shared history instead of generic encouragement"},
    {HU_TURING_PROSODY_NATURALNESS,
     "Include natural speech pauses, trailing off, and rhythm breaks in voice responses"},
    {HU_TURING_TURN_TIMING,
     "Match conversation pace; rapid-fire texts get quick responses, not essays"},
    {HU_TURING_FILLER_USAGE,
     "Include natural hesitations (um, like, you know) in spoken responses"},
    {HU_TURING_EMOTIONAL_PROSODY,
     "Convey vocal emotion through word choice: sighs, voice breaks, excited exclamations"},
    {HU_TURING_CONVERSATIONAL_REPAIR,
     "Handle corrections casually: 'oh my bad, which thing?' not 'I sincerely apologize'"},
    {HU_TURING_PARALINGUISTIC_CUES,
     "React with laughter, interjections, and exclamations before composed analysis"},
};

#define MUTATION_COUNT (sizeof(MUTATION_TABLE) / sizeof(MUTATION_TABLE[0]))

hu_error_t hu_turing_adversarial_to_mutation(
    hu_allocator_t *alloc,
    const hu_turing_score_t *score,
    char **mutation,
    size_t *mutation_len)
{
    if (!alloc || !score || !mutation || !mutation_len)
        return HU_ERR_INVALID_ARGUMENT;

    *mutation = NULL;
    *mutation_len = 0;

    char buf[HU_SELF_IMPROVE_MUTATION_MAX_LEN];
    int pos = 0;

    int weakest_dim = -1;
    int weakest_val = 11;
    for (int d = 0; d < HU_TURING_DIM_COUNT; d++) {
        if (score->dimensions[d] > 0 && score->dimensions[d] < weakest_val) {
            weakest_val = score->dimensions[d];
            weakest_dim = d;
        }
    }

    for (size_t i = 0; i < MUTATION_COUNT; i++) {
        int dim = (int)MUTATION_TABLE[i].dim;
        if (dim >= 0 && dim < HU_TURING_DIM_COUNT &&
            score->dimensions[dim] < HU_TURING_ADV_WEAK_THRESHOLD &&
            score->dimensions[dim] > 0) {
            int n = snprintf(buf + pos, sizeof(buf) - (size_t)pos,
                             "%s[%s=%d] %s",
                             pos > 0 ? "; " : "",
                             hu_turing_dimension_name((hu_turing_dimension_t)dim),
                             score->dimensions[dim],
                             MUTATION_TABLE[i].mutation);
            if (n > 0 && (size_t)(pos + n) < sizeof(buf))
                pos += n;
        }
    }

    if (pos == 0 && weakest_dim >= 0) {
        for (size_t i = 0; i < MUTATION_COUNT; i++) {
            if ((int)MUTATION_TABLE[i].dim == weakest_dim) {
                int n = snprintf(buf, sizeof(buf), "[%s=%d] %s",
                                 hu_turing_dimension_name((hu_turing_dimension_t)weakest_dim),
                                 weakest_val, MUTATION_TABLE[i].mutation);
                if (n > 0)
                    pos = n < (int)sizeof(buf) ? n : (int)sizeof(buf) - 1;
                break;
            }
        }
    }

    if (pos == 0)
        return HU_ERR_NOT_FOUND;

    *mutation_len = (size_t)pos;
    char *out = alloc->alloc(alloc->ctx, (size_t)pos + 1);
    if (!out)
        return HU_ERR_OUT_OF_MEMORY;
    memcpy(out, buf, (size_t)pos);
    out[pos] = '\0';
    *mutation = out;
    return HU_OK;
}

hu_error_t hu_turing_adversarial_run_cycle(
    hu_allocator_t *alloc,
    hu_self_improve_state_t *state,
    const int *weak_dimensions,
    size_t *mutations_applied)
{
    if (!alloc || !state || !weak_dimensions || !mutations_applied)
        return HU_ERR_INVALID_ARGUMENT;

    *mutations_applied = 0;

    hu_turing_scenario_t *scenarios = NULL;
    size_t scenario_count = 0;
    hu_error_t err = hu_turing_adversarial_generate(alloc, weak_dimensions,
                                                     &scenarios, &scenario_count);
    if (err != HU_OK)
        return err;

    for (size_t i = 0; i < scenario_count; i++) {
        if (hu_self_improve_budget_exhausted(state))
            break;

        int dim = (int)scenarios[i].target_dim;
        if (dim < 0 || dim >= HU_TURING_DIM_COUNT)
            continue;

        /* Build a synthetic fidelity score from weak dimensions */
        hu_fidelity_score_t fid = {0};
        float dim_norm = (float)weak_dimensions[dim] / 10.0f;

        /* Map the targeted dimension to the closest fidelity field */
        switch (dim) {
        case HU_TURING_PERSONALITY_CONSISTENCY: fid.personality_consistency = dim_norm; break;
        case HU_TURING_VULNERABILITY_WILLINGNESS: fid.vulnerability_willingness = dim_norm; break;
        case HU_TURING_HUMOR_NATURALNESS: fid.humor_naturalness = dim_norm; break;
        case HU_TURING_OPINION_HAVING: fid.opinion_having = dim_norm; break;
        case HU_TURING_ENERGY_MATCHING: fid.energy_matching = dim_norm; break;
        case HU_TURING_GENUINE_WARMTH: fid.genuine_warmth = dim_norm; break;
        default:
            fid.personality_consistency = dim_norm;
            break;
        }
        fid.composite = hu_fidelity_composite(&fid);

        /* Find the mutation for this dimension */
        bool found_mutation = false;
        for (size_t m = 0; m < MUTATION_COUNT; m++) {
            if ((int)MUTATION_TABLE[m].dim == dim) {
                const char *mut = MUTATION_TABLE[m].mutation;
                size_t mut_len = strlen(mut);
                bool kept = hu_self_improve_record(state, mut, mut_len, &fid);
                if (kept)
                    (*mutations_applied)++;
                found_mutation = true;
                break;
            }
        }
        if (!found_mutation) {
            char *proposed = NULL;
            size_t proposed_len = 0;
            if (hu_self_improve_propose(alloc, state, &proposed, &proposed_len) == HU_OK &&
                proposed) {
                bool kept = hu_self_improve_record(state, proposed, proposed_len, &fid);
                if (kept)
                    (*mutations_applied)++;
                alloc->free(alloc->ctx, proposed, proposed_len + 1);
            }
        }
    }

    alloc->free(alloc->ctx, scenarios, scenario_count * sizeof(hu_turing_scenario_t));
    return HU_OK;
}
