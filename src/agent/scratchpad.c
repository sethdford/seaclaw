#include "human/agent/scratchpad.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

void hu_scratchpad_init(hu_scratchpad_t *sp, size_t max_bytes) {
    if (!sp)
        return;
    memset(sp, 0, sizeof(*sp));
    sp->max_bytes = max_bytes;
}

static hu_scratchpad_entry_t *find_entry(const hu_scratchpad_t *sp, const char *key,
                                         size_t key_len) {
    for (size_t i = 0; i < sp->entry_count; i++) {
        if (strlen(sp->entries[i].key) == key_len && memcmp(sp->entries[i].key, key, key_len) == 0)
            return (hu_scratchpad_entry_t *)&sp->entries[i];
    }
    return NULL;
}

hu_error_t hu_scratchpad_set(hu_scratchpad_t *sp, hu_allocator_t *alloc, const char *key,
                             size_t key_len, const char *value, size_t value_len) {
    if (!sp || !alloc || !key)
        return HU_ERR_INVALID_ARGUMENT;
    if (key_len == 0 || key_len >= 128)
        return HU_ERR_INVALID_ARGUMENT;

    hu_scratchpad_entry_t *existing = find_entry(sp, key, key_len);
    if (existing) {
        sp->total_bytes -= existing->value_len;
        if (existing->value) {
            alloc->free(alloc->ctx, existing->value, existing->value_len + 1);
            existing->value = NULL;
            existing->value_len = 0;
        }
    }

    if (sp->max_bytes > 0 && sp->total_bytes + value_len > sp->max_bytes)
        return HU_ERR_OUT_OF_MEMORY;

    if (!existing) {
        if (sp->entry_count >= HU_SCRATCHPAD_MAX_ENTRIES)
            return HU_ERR_OUT_OF_MEMORY;
        existing = &sp->entries[sp->entry_count++];
        memset(existing, 0, sizeof(*existing));
        memcpy(existing->key, key, key_len);
        existing->key[key_len] = '\0';
        existing->created_at = (int64_t)time(NULL);
    }

    if (value && value_len > 0) {
        existing->value = (char *)alloc->alloc(alloc->ctx, value_len + 1);
        if (!existing->value)
            return HU_ERR_OUT_OF_MEMORY;
        memcpy(existing->value, value, value_len);
        existing->value[value_len] = '\0';
        existing->value_len = value_len;
    }

    existing->updated_at = (int64_t)time(NULL);
    existing->access_count++;
    sp->total_bytes += value_len;
    return HU_OK;
}

hu_error_t hu_scratchpad_get(const hu_scratchpad_t *sp, const char *key, size_t key_len,
                             const char **value_out, size_t *value_len_out) {
    if (!sp || !key || !value_out)
        return HU_ERR_INVALID_ARGUMENT;

    hu_scratchpad_entry_t *entry = find_entry(sp, key, key_len);
    if (!entry)
        return HU_ERR_NOT_FOUND;

    entry->access_count++;
    *value_out = entry->value;
    if (value_len_out)
        *value_len_out = entry->value_len;
    return HU_OK;
}

hu_error_t hu_scratchpad_delete(hu_scratchpad_t *sp, hu_allocator_t *alloc, const char *key,
                                size_t key_len) {
    if (!sp || !alloc || !key)
        return HU_ERR_INVALID_ARGUMENT;

    for (size_t i = 0; i < sp->entry_count; i++) {
        if (strlen(sp->entries[i].key) == key_len &&
            memcmp(sp->entries[i].key, key, key_len) == 0) {
            sp->total_bytes -= sp->entries[i].value_len;
            if (sp->entries[i].value)
                alloc->free(alloc->ctx, sp->entries[i].value, sp->entries[i].value_len + 1);
            /* Shift remaining entries */
            for (size_t j = i; j < sp->entry_count - 1; j++)
                sp->entries[j] = sp->entries[j + 1];
            sp->entry_count--;
            return HU_OK;
        }
    }
    return HU_ERR_NOT_FOUND;
}

bool hu_scratchpad_has(const hu_scratchpad_t *sp, const char *key, size_t key_len) {
    return sp && find_entry(sp, key, key_len) != NULL;
}

size_t hu_scratchpad_to_json(const hu_scratchpad_t *sp, char *buf, size_t buf_size) {
    if (!sp || !buf || buf_size == 0)
        return 0;
    size_t pos = 0;
    if (pos < buf_size - 1)
        buf[pos++] = '{';

    for (size_t i = 0; i < sp->entry_count; i++) {
        if (i > 0 && pos < buf_size - 1)
            buf[pos++] = ',';
        int n = snprintf(buf + pos, buf_size - pos, "\"%s\":\"%.*s\"", sp->entries[i].key,
                         (int)(sp->entries[i].value_len < 256 ? sp->entries[i].value_len : 256),
                         sp->entries[i].value ? sp->entries[i].value : "");
        if (n > 0 && pos + (size_t)n < buf_size)
            pos += (size_t)n;
        else
            break;
    }

    if (pos < buf_size - 1)
        buf[pos++] = '}';
    buf[pos] = '\0';
    return pos;
}

void hu_scratchpad_clear(hu_scratchpad_t *sp, hu_allocator_t *alloc) {
    if (!sp || !alloc)
        return;
    for (size_t i = 0; i < sp->entry_count; i++) {
        if (sp->entries[i].value)
            alloc->free(alloc->ctx, sp->entries[i].value, sp->entries[i].value_len + 1);
    }
    sp->entry_count = 0;
    sp->total_bytes = 0;
}

void hu_scratchpad_deinit(hu_scratchpad_t *sp, hu_allocator_t *alloc) {
    hu_scratchpad_clear(sp, alloc);
}
