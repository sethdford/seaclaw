#ifndef HU_DATA_LOADER_H
#define HU_DATA_LOADER_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>

/* Set custom data directory (overrides ~/.human/data/). Thread-safe for single init. */
void hu_data_set_dir(const char *dir);

/* Load a data file by relative path (e.g. "prompts/group_chat_hint.txt").
 * First checks ~/.human/data/<path> for a user override.
 * Falls back to the compiled-in embedded default.
 * Returns an allocated copy — caller owns and must free. */
hu_error_t hu_data_load(hu_allocator_t *alloc, const char *relative_path,
                        char **out, size_t *out_len);

/* Load embedded default only (no filesystem check). */
hu_error_t hu_data_load_embedded(hu_allocator_t *alloc, const char *relative_path,
                                 char **out, size_t *out_len);

#endif
