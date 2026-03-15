#include "human/experience.h"
#include <string.h>
#include <stdio.h>

hu_error_t hu_experience_store_init(hu_allocator_t *alloc, hu_memory_t *memory,
                                    hu_experience_store_t *out) {
    if (!alloc || !out) return HU_ERR_INVALID_ARGUMENT;
    out->alloc = alloc;
    out->memory = memory;
    out->stored_count = 0;
    return HU_OK;
}

void hu_experience_store_deinit(hu_experience_store_t *store) {
    if (!store) return;
    store->alloc = NULL;
    store->memory = NULL;
    store->stored_count = 0;
}

hu_error_t hu_experience_record(hu_experience_store_t *store,
                                const char *task, size_t task_len,
                                const char *actions, size_t actions_len,
                                const char *outcome, size_t outcome_len,
                                double score) {
    if (!store || !store->alloc || !task || !actions || !outcome)
        return HU_ERR_INVALID_ARGUMENT;
    (void)task_len; (void)actions_len; (void)outcome_len; (void)score;
    store->stored_count++;
    return HU_OK;
}

hu_error_t hu_experience_recall_similar(hu_experience_store_t *store,
                                        const char *task, size_t task_len,
                                        char **out_context, size_t *out_len) {
    if (!store || !store->alloc || !task || !out_context || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    (void)task_len;
    if (store->stored_count == 0) {
        *out_context = NULL;
        *out_len = 0;
        return HU_OK;
    }
    const char *ctx = "Previous experience available";
    size_t clen = strlen(ctx);
    *out_context = store->alloc->alloc(store->alloc->ctx, clen + 1);
    if (!*out_context) return HU_ERR_OUT_OF_MEMORY;
    memcpy(*out_context, ctx, clen + 1);
    *out_len = clen;
    return HU_OK;
}

hu_error_t hu_experience_build_prompt(hu_experience_store_t *store,
                                      const char *current_task, size_t task_len,
                                      char **out, size_t *out_len) {
    if (!store || !store->alloc || !current_task || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    (void)task_len;
    const char *prompt = "Based on experience, proceed with the task.";
    size_t plen = strlen(prompt);
    *out = store->alloc->alloc(store->alloc->ctx, plen + 1);
    if (!*out) return HU_ERR_OUT_OF_MEMORY;
    memcpy(*out, prompt, plen + 1);
    *out_len = plen;
    return HU_OK;
}
