#ifndef HU_AGENT_HULA_ANALYTICS_H
#define HU_AGENT_HULA_ANALYTICS_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>

/* Summarize persisted HuLa trace JSON files in trace_dir (e.g. ~/.human/hula_traces).
 * On success, *out_json is an object with: file_count, success_count, fail_count,
 * total_trace_steps, newest_ts (unix seconds, or 0). Caller frees *out_json. */
hu_error_t hu_hula_analytics_summarize(hu_allocator_t *alloc, const char *trace_dir, char **out_json,
                                       size_t *out_len);

#endif /* HU_AGENT_HULA_ANALYTICS_H */
