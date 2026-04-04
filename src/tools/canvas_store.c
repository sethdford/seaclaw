#include "human/tools/canvas_store.h"
#include "human/core/string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CANVAS_MAX          64
#define CANVAS_MAX_VERSIONS 32

typedef struct {
    char *content;
    uint32_t seq;
} canvas_version_t;

typedef struct {
    char *canvas_id;
    char *format;
    char *language;
    char *imports_json;
    char *title;
    char *content;
    canvas_version_t versions[CANVAS_MAX_VERSIONS];
    uint32_t version_head;
    uint32_t version_count;
    uint32_t version_seq;
    uint32_t undo_pos;
    bool user_edit_pending;
} canvas_entry_t;

struct hu_canvas_store {
    hu_allocator_t *alloc;
    canvas_entry_t entries[CANVAS_MAX];
    size_t count;
    uint32_t next_id;
    void *db;
};

static void entry_free(hu_allocator_t *a, canvas_entry_t *e) {
    if (e->canvas_id)
        a->free(a->ctx, e->canvas_id, strlen(e->canvas_id) + 1);
    if (e->format)
        a->free(a->ctx, e->format, strlen(e->format) + 1);
    if (e->language)
        a->free(a->ctx, e->language, strlen(e->language) + 1);
    if (e->imports_json)
        a->free(a->ctx, e->imports_json, strlen(e->imports_json) + 1);
    if (e->title)
        a->free(a->ctx, e->title, strlen(e->title) + 1);
    if (e->content)
        a->free(a->ctx, e->content, strlen(e->content) + 1);
    for (uint32_t i = 0; i < CANVAS_MAX_VERSIONS; i++) {
        if (e->versions[i].content)
            a->free(a->ctx, e->versions[i].content, strlen(e->versions[i].content) + 1);
    }
    memset(e, 0, sizeof(*e));
}

static void entry_fill_info(const canvas_entry_t *e, hu_canvas_info_t *out) {
    out->canvas_id = e->canvas_id;
    out->format = e->format;
    out->language = e->language;
    out->imports = e->imports_json;
    out->title = e->title;
    out->content = e->content;
    out->version_seq = e->version_seq;
    out->version_count = e->version_count;
    out->user_edit_pending = e->user_edit_pending;
}

static canvas_entry_t *store_find_entry(hu_canvas_store_t *s, const char *id) {
    if (!s || !id)
        return NULL;
    for (size_t i = 0; i < s->count; i++) {
        if (s->entries[i].canvas_id && strcmp(s->entries[i].canvas_id, id) == 0)
            return &s->entries[i];
    }
    return NULL;
}

static void push_version(hu_allocator_t *alloc, canvas_entry_t *e) {
    uint32_t slot = e->version_head % CANVAS_MAX_VERSIONS;
    if (e->versions[slot].content)
        alloc->free(alloc->ctx, e->versions[slot].content,
                    strlen(e->versions[slot].content) + 1);
    e->versions[slot].content = e->content ? hu_strndup(alloc, e->content, strlen(e->content)) : NULL;
    e->versions[slot].seq = e->version_seq;
    e->version_head++;
    if (e->version_count < CANVAS_MAX_VERSIONS)
        e->version_count++;
    e->undo_pos = 0;
}

hu_canvas_store_t *hu_canvas_store_create(hu_allocator_t *alloc) {
    if (!alloc)
        return NULL;
    hu_canvas_store_t *s = (hu_canvas_store_t *)alloc->alloc(alloc->ctx, sizeof(*s));
    if (!s)
        return NULL;
    memset(s, 0, sizeof(*s));
    s->alloc = alloc;
    return s;
}

void hu_canvas_store_destroy(hu_canvas_store_t *store) {
    if (!store)
        return;
    hu_allocator_t *a = store->alloc;
    for (size_t i = 0; i < store->count; i++)
        entry_free(a, &store->entries[i]);
    a->free(a->ctx, store, sizeof(*store));
}

size_t hu_canvas_store_count(hu_canvas_store_t *store) {
    return store ? store->count : 0;
}

hu_error_t hu_canvas_store_put_canvas(hu_canvas_store_t *store, const char *canvas_id,
                                      const char *format, const char *imports,
                                      const char *language, const char *title,
                                      const char *content) {
    if (!store || !canvas_id)
        return HU_ERR_INVALID_ARGUMENT;
    if (store->count >= CANVAS_MAX)
        return HU_ERR_INTERNAL;

    canvas_entry_t *e = &store->entries[store->count];
    memset(e, 0, sizeof(*e));
    hu_allocator_t *a = store->alloc;

    e->canvas_id = hu_strndup(a, canvas_id, strlen(canvas_id));
    if (!e->canvas_id)
        return HU_ERR_OUT_OF_MEMORY;
    e->format = format ? hu_strndup(a, format, strlen(format)) : NULL;
    e->language = language ? hu_strndup(a, language, strlen(language)) : NULL;
    e->imports_json = imports ? hu_strndup(a, imports, strlen(imports)) : NULL;
    e->title = title ? hu_strndup(a, title, strlen(title)) : NULL;
    e->content = content ? hu_strndup(a, content, strlen(content)) : hu_strndup(a, "", 0);
    store->count++;

    if (store->db) {
        hu_canvas_persist_save(store->db, canvas_id, format, imports, language, title, content);
    }
    return HU_OK;
}

void hu_canvas_store_remove_canvas(hu_canvas_store_t *store, const char *canvas_id) {
    if (!store || !canvas_id)
        return;
    for (size_t i = 0; i < store->count; i++) {
        if (store->entries[i].canvas_id && strcmp(store->entries[i].canvas_id, canvas_id) == 0) {
            if (store->db)
                hu_canvas_persist_delete(store->db, canvas_id);
            entry_free(store->alloc, &store->entries[i]);
            if (i + 1 < store->count)
                store->entries[i] = store->entries[store->count - 1];
            memset(&store->entries[store->count - 1], 0, sizeof(canvas_entry_t));
            store->count--;
            return;
        }
    }
}

bool hu_canvas_store_find(hu_canvas_store_t *store, const char *canvas_id, hu_canvas_info_t *out) {
    canvas_entry_t *e = store_find_entry(store, canvas_id);
    if (!e)
        return false;
    if (out)
        entry_fill_info(e, out);
    return true;
}

bool hu_canvas_store_get(hu_canvas_store_t *store, size_t index, hu_canvas_info_t *out) {
    if (!store || index >= store->count)
        return false;
    if (out)
        entry_fill_info(&store->entries[index], out);
    return true;
}

hu_error_t hu_canvas_store_agent_update(hu_canvas_store_t *store, const char *canvas_id,
                                        const char *content) {
    if (!store || !canvas_id || !content)
        return HU_ERR_INVALID_ARGUMENT;
    canvas_entry_t *e = store_find_entry(store, canvas_id);
    if (!e)
        return HU_ERR_NOT_FOUND;

    push_version(store->alloc, e);
    if (e->content)
        store->alloc->free(store->alloc->ctx, e->content, strlen(e->content) + 1);
    e->content = hu_strndup(store->alloc, content, strlen(content));
    e->version_seq++;
    e->user_edit_pending = false;

    if (store->db) {
        hu_canvas_persist_save(store->db, canvas_id, e->format, e->imports_json,
                               e->language, e->title, content);
        hu_canvas_persist_save_version(store->db, canvas_id, e->version_seq, content);
    }
    return HU_OK;
}

hu_error_t hu_canvas_store_edit(hu_canvas_store_t *store, const char *canvas_id,
                                const char *content) {
    if (!store || !canvas_id || !content)
        return HU_ERR_INVALID_ARGUMENT;
    canvas_entry_t *e = store_find_entry(store, canvas_id);
    if (!e)
        return HU_ERR_NOT_FOUND;

    push_version(store->alloc, e);
    if (e->content)
        store->alloc->free(store->alloc->ctx, e->content, strlen(e->content) + 1);
    e->content = hu_strndup(store->alloc, content, strlen(content));
    e->version_seq++;
    e->user_edit_pending = true;

    if (store->db) {
        hu_canvas_persist_save(store->db, canvas_id, e->format, e->imports_json,
                               e->language, e->title, content);
        hu_canvas_persist_save_version(store->db, canvas_id, e->version_seq, content);
    }
    return HU_OK;
}

hu_error_t hu_canvas_store_undo(hu_canvas_store_t *store, const char *canvas_id,
                                hu_canvas_info_t *out) {
    if (!store || !canvas_id)
        return HU_ERR_INVALID_ARGUMENT;
    canvas_entry_t *e = store_find_entry(store, canvas_id);
    if (!e)
        return HU_ERR_NOT_FOUND;
    if (e->undo_pos + 1 >= e->version_count)
        return HU_ERR_NOT_FOUND;

    e->undo_pos++;
    uint32_t slot = (e->version_head - 1 - e->undo_pos) % CANVAS_MAX_VERSIONS;
    if (!e->versions[slot].content)
        return HU_ERR_NOT_FOUND;

    if (e->content)
        store->alloc->free(store->alloc->ctx, e->content, strlen(e->content) + 1);
    e->content = hu_strndup(store->alloc, e->versions[slot].content,
                            strlen(e->versions[slot].content));
    if (out)
        entry_fill_info(e, out);
    return HU_OK;
}

hu_error_t hu_canvas_store_redo(hu_canvas_store_t *store, const char *canvas_id,
                                hu_canvas_info_t *out) {
    if (!store || !canvas_id)
        return HU_ERR_INVALID_ARGUMENT;
    canvas_entry_t *e = store_find_entry(store, canvas_id);
    if (!e)
        return HU_ERR_NOT_FOUND;
    if (e->undo_pos == 0)
        return HU_ERR_NOT_FOUND;

    e->undo_pos--;
    uint32_t slot;
    if (e->undo_pos == 0) {
        slot = (e->version_head - 1) % CANVAS_MAX_VERSIONS;
    } else {
        slot = (e->version_head - 1 - e->undo_pos) % CANVAS_MAX_VERSIONS;
    }
    if (!e->versions[slot].content)
        return HU_ERR_NOT_FOUND;

    if (e->content)
        store->alloc->free(store->alloc->ctx, e->content, strlen(e->content) + 1);
    e->content = hu_strndup(store->alloc, e->versions[slot].content,
                            strlen(e->versions[slot].content));
    if (out)
        entry_fill_info(e, out);
    return HU_OK;
}

void hu_canvas_store_set_db_internal(hu_canvas_store_t *store, void *db) {
    if (store)
        store->db = db;
}
