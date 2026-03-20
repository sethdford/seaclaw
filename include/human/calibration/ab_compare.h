#ifndef HU_CALIBRATION_AB_COMPARE_H
#define HU_CALIBRATION_AB_COMPARE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/provider.h"
#include <stddef.h>

/* Compare twin-generated reply vs a real human reply for the same context.
 * When provider is non-NULL, uses LLM-as-judge (HU_EVAL_LLM_JUDGE style check);
 * otherwise uses a lightweight heuristic (test-friendly).
 * Sets *twin_preferred when the twin reply is scored as more natural than real. */
hu_error_t hu_calibrate_ab_compare(hu_allocator_t *alloc, hu_provider_t *provider, const char *model,
                                   size_t model_len, const char *twin_reply, size_t twin_len,
                                   const char *real_reply, size_t real_len, bool *twin_preferred,
                                   double *twin_score_out, double *real_score_out);

#endif /* HU_CALIBRATION_AB_COMPARE_H */
