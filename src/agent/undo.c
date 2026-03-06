#include "seaclaw/agent/undo.h"
#include "seaclaw/core/string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

struct sc_undo_stack {
    sc_allocator_t *alloc;
    sc_undo_entry_t *entries;
    size_t capacity;
    size_t count;
    size_t head; /* next write position (ring buffer) */
};

sc_undo_stack_t *sc_undo_stack_create(sc_allocator_t *alloc, size_t max_entries) {
    if (!alloc || max_entries == 0)
        return NULL;
    sc_undo_stack_t *stack = (sc_undo_stack_t *)alloc->alloc(alloc->ctx, sizeof(sc_undo_stack_t));
    if (!stack)
        return NULL;
    memset(stack, 0, sizeof(*stack));
    stack->alloc = alloc;
    stack->capacity = max_entries;
    stack->entries =
        (sc_undo_entry_t *)alloc->alloc(alloc->ctx, max_entries * sizeof(sc_undo_entry_t));
    if (!stack->entries) {
        alloc->free(alloc->ctx, stack, sizeof(sc_undo_stack_t));
        return NULL;
    }
    memset(stack->entries, 0, max_entries * sizeof(sc_undo_entry_t));
    return stack;
}

static void free_entry(sc_allocator_t *alloc, sc_undo_entry_t *e) {
    if (!e)
        return;
    if (e->description) {
        alloc->free(alloc->ctx, e->description, strlen(e->description) + 1);
        e->description = NULL;
    }
    if (e->path) {
        alloc->free(alloc->ctx, e->path, strlen(e->path) + 1);
        e->path = NULL;
    }
    if (e->original_content) {
        alloc->free(alloc->ctx, e->original_content, e->original_content_len + 1);
        e->original_content = NULL;
    }
}

void sc_undo_stack_destroy(sc_undo_stack_t *stack) {
    if (!stack || !stack->alloc)
        return;
    for (size_t i = 0; i < stack->capacity; i++)
        free_entry(stack->alloc, &stack->entries[i]);
    stack->alloc->free(stack->alloc->ctx, stack->entries,
                       stack->capacity * sizeof(sc_undo_entry_t));
    stack->alloc->free(stack->alloc->ctx, stack, sizeof(sc_undo_stack_t));
}

sc_error_t sc_undo_stack_push(sc_undo_stack_t *stack, const sc_undo_entry_t *entry) {
    if (!stack || !entry)
        return SC_ERR_INVALID_ARGUMENT;
    size_t idx = stack->head % stack->capacity;
    free_entry(stack->alloc, &stack->entries[idx]);

    stack->entries[idx].type = entry->type;
    stack->entries[idx].timestamp = entry->timestamp;
    stack->entries[idx].reversible = entry->reversible;
    stack->entries[idx].original_content_len = entry->original_content_len;

    stack->entries[idx].description =
        entry->description ? sc_strdup(stack->alloc, entry->description) : NULL;
    stack->entries[idx].path = entry->path ? sc_strdup(stack->alloc, entry->path) : NULL;
    stack->entries[idx].original_content = NULL;
    if (entry->original_content && entry->original_content_len > 0) {
        stack->entries[idx].original_content =
            (char *)stack->alloc->alloc(stack->alloc->ctx, entry->original_content_len + 1);
        if (stack->entries[idx].original_content) {
            memcpy(stack->entries[idx].original_content, entry->original_content,
                   entry->original_content_len);
            stack->entries[idx].original_content[entry->original_content_len] = '\0';
        }
    }

    stack->head++;
    if (stack->count < stack->capacity)
        stack->count++;
    return SC_OK;
}

size_t sc_undo_stack_count(const sc_undo_stack_t *stack) {
    return stack ? stack->count : 0;
}

sc_error_t sc_undo_stack_execute_undo(sc_undo_stack_t *stack, sc_allocator_t *alloc) {
    if (!stack || !alloc || stack->count == 0)
        return SC_ERR_INVALID_ARGUMENT;

    size_t idx = (stack->head - 1) % stack->capacity;
    sc_undo_entry_t *e = &stack->entries[idx];

#if defined(SC_IS_TEST) && SC_IS_TEST
    (void)e;
    /* Skip actual I/O in tests; still pop the entry */
#else
    if (e->path) {
        if (e->type == SC_UNDO_FILE_WRITE && e->original_content) {
            FILE *f = fopen(e->path, "wb");
            if (f) {
                size_t n = fwrite(e->original_content, 1, e->original_content_len, f);
                fclose(f);
                if (n != e->original_content_len) {
                    return SC_ERR_IO;
                }
            } else {
                return SC_ERR_IO;
            }
        } else if (e->type == SC_UNDO_FILE_CREATE) {
#if defined(__unix__) || defined(__APPLE__)
            if (unlink(e->path) != 0)
                return SC_ERR_IO;
#endif
        }
    }
#endif

    free_entry(stack->alloc, e);
    memset(e, 0, sizeof(*e));
    stack->head = (stack->head > 0) ? stack->head - 1 : stack->capacity - 1;
    stack->count = (stack->count > 0) ? stack->count - 1 : 0;
    return SC_OK;
}

void sc_undo_entry_free(sc_allocator_t *alloc, sc_undo_entry_t *entry) {
    if (!alloc || !entry)
        return;
    free_entry(alloc, entry);
}
