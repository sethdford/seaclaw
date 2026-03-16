#ifndef HU_INTELLIGENCE_SKILLS_CONTEXT_H
#define HU_INTELLIGENCE_SKILLS_CONTEXT_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>

/* Build contact-scoped learned skills context for prompt injection.
 * Returns formatted string (caller frees). Does not pull in hu_skill_t. */
hu_error_t hu_skill_build_contact_context(hu_allocator_t *alloc, sqlite3 *db,
                                          const char *contact_id, size_t cid_len, char **out,
                                          size_t *out_len);
#endif

#endif /* HU_INTELLIGENCE_SKILLS_CONTEXT_H */
