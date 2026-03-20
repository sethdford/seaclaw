#include "human/calibration/ab_compare.h"
#include "human/eval.h"
#include <string.h>

hu_error_t hu_calibrate_ab_compare(hu_allocator_t *alloc, hu_provider_t *provider, const char *model,
                                   size_t model_len, const char *twin_reply, size_t twin_len,
                                   const char *real_reply, size_t real_len, bool *twin_preferred,
                                   double *twin_score_out, double *real_score_out) {
    if (!alloc || !twin_preferred)
        return HU_ERR_INVALID_ARGUMENT;
    *twin_preferred = false;
    if (twin_score_out)
        *twin_score_out = 0.0;
    if (real_score_out)
        *real_score_out = 0.0;
    if (!twin_reply || twin_len == 0 || !real_reply || real_len == 0)
        return HU_ERR_INVALID_ARGUMENT;

#if HU_IS_TEST
    (void)provider;
    (void)model;
    (void)model_len;
    /* Deterministic stub: prefer shorter reply as "more concise human". */
    *twin_preferred = twin_len < real_len;
    if (twin_score_out)
        *twin_score_out = *twin_preferred ? 0.9 : 0.4;
    if (real_score_out)
        *real_score_out = *twin_preferred ? 0.4 : 0.9;
    return HU_OK;
#else
    static const char judge_tpl[] =
        "You compare two replies to the same chat message. Reply A is from a digital twin; "
        "Reply B is what the real human actually sent. Which feels more natural and human for "
        "a close-friend text thread? Answer with exactly one letter: A or B.\n\nReply A:\n";
    static const char mid[] = "\n\nReply B:\n";
    static const char tail[] = "\n\nAnswer (A or B only):";
    size_t tpl_len = sizeof(judge_tpl) - 1;
    size_t mid_len = sizeof(mid) - 1;
    size_t tail_len = sizeof(tail) - 1;
    size_t total = tpl_len + twin_len + mid_len + real_len + tail_len;
    char *prompt = (char *)alloc->alloc(alloc->ctx, total + 1);
    if (!prompt)
        return HU_ERR_OUT_OF_MEMORY;
    size_t pos = 0;
    memcpy(prompt + pos, judge_tpl, tpl_len);
    pos += tpl_len;
    memcpy(prompt + pos, twin_reply, twin_len);
    pos += twin_len;
    memcpy(prompt + pos, mid, mid_len);
    pos += mid_len;
    memcpy(prompt + pos, real_reply, real_len);
    pos += real_len;
    memcpy(prompt + pos, tail, tail_len);
    pos += tail_len;
    prompt[pos] = '\0';

    if (provider && model && model_len > 0 && provider->vtable) {
        bool a_wins = false;
        double sc = 0.0;
        hu_error_t err =
            hu_eval_check_with_provider(alloc, prompt, pos, "A", 1, HU_EVAL_LLM_JUDGE, provider,
                                        model, model_len, &a_wins, &sc);
        alloc->free(alloc->ctx, prompt, total + 1);
        if (err != HU_OK)
            return err;
        *twin_preferred = a_wins;
        if (twin_score_out)
            *twin_score_out = a_wins ? sc : (1.0 - sc);
        if (real_score_out)
            *real_score_out = a_wins ? (1.0 - sc) : sc;
        return HU_OK;
    }

    alloc->free(alloc->ctx, prompt, total + 1);
    /* No provider: lightweight length heuristic (not authoritative). */
    *twin_preferred = (twin_len > 0 && twin_len + 8 < real_len);
    if (twin_score_out)
        *twin_score_out = *twin_preferred ? 0.55 : 0.45;
    if (real_score_out)
        *real_score_out = *twin_preferred ? 0.45 : 0.55;
    return HU_OK;
#endif
}
