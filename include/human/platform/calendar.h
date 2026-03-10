#ifndef HU_PLATFORM_CALENDAR_H
#define HU_PLATFORM_CALENDAR_H

#include "human/core/allocator.h"
#include "human/core/error.h"

/* Query macOS Calendar for events in the next hours_ahead hours.
 * Returns JSON array string (caller frees via alloc->free).
 * On non-macOS: returns HU_ERR_NOT_SUPPORTED.
 * In HU_IS_TEST: returns empty array "[]" without spawning. */
hu_error_t hu_calendar_macos_get_events(hu_allocator_t *alloc, int hours_ahead,
                                       char **events_json, size_t *events_len);

#endif
