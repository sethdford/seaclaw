#ifndef HU_PWA_CONTEXT_H
#define HU_PWA_CONTEXT_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/pwa.h"

#define HU_PWA_CONTEXT_MAX_LEN 4096

/* Build a cross-app context string by reading all open PWA tabs.
 * Output is a formatted string like:
 *   "[Calendar] 3 events today: 9am standup, 2pm review, 4:30pm 1:1\n"
 *   "[Gmail] 5 unread: Q1 report from bob, meeting prep from alice\n"
 *   "[Slack] Recent: alice in #general: PR is up for review\n"
 * Caller must free *out with alloc->free(ctx, *out, *out_len + 1) */
hu_error_t hu_pwa_context_build(hu_allocator_t *alloc, char **out, size_t *out_len);

#endif /* HU_PWA_CONTEXT_H */
