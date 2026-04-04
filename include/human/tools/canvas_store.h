#ifndef HU_TOOLS_CANVAS_STORE_H
#define HU_TOOLS_CANVAS_STORE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct hu_canvas_store hu_canvas_store_t;

typedef struct hu_canvas_info {
    const char *canvas_id;
    const char *format;
    const char *language;
    const char *imports;
    const char *title;
    const char *content;
    uint32_t version_seq;
    uint32_t version_count;
    bool user_edit_pending;
} hu_canvas_info_t;

hu_canvas_store_t *hu_canvas_store_create(hu_allocator_t *alloc);
void hu_canvas_store_destroy(hu_canvas_store_t *store);
size_t hu_canvas_store_count(hu_canvas_store_t *store);

hu_error_t hu_canvas_store_put_canvas(hu_canvas_store_t *store, const char *canvas_id,
                                      const char *format, const char *imports,
                                      const char *language, const char *title,
                                      const char *content);
void hu_canvas_store_remove_canvas(hu_canvas_store_t *store, const char *canvas_id);

bool hu_canvas_store_find(hu_canvas_store_t *store, const char *canvas_id, hu_canvas_info_t *out);
bool hu_canvas_store_get(hu_canvas_store_t *store, size_t index, hu_canvas_info_t *out);

hu_error_t hu_canvas_store_agent_update(hu_canvas_store_t *store, const char *canvas_id,
                                        const char *content);
hu_error_t hu_canvas_store_edit(hu_canvas_store_t *store, const char *canvas_id,
                                const char *content);
hu_error_t hu_canvas_store_undo(hu_canvas_store_t *store, const char *canvas_id,
                                hu_canvas_info_t *out);
hu_error_t hu_canvas_store_redo(hu_canvas_store_t *store, const char *canvas_id,
                                hu_canvas_info_t *out);

void hu_canvas_store_set_db_internal(hu_canvas_store_t *store, void *db);

hu_error_t hu_canvas_store_set_db(hu_canvas_store_t *store, void *db);
hu_error_t hu_canvas_persist_save(void *db, const char *canvas_id, const char *format,
                                  const char *imports, const char *language, const char *title,
                                  const char *content);
hu_error_t hu_canvas_persist_save_version(void *db, const char *canvas_id, uint32_t version_seq,
                                          const char *content);
hu_error_t hu_canvas_persist_delete(void *db, const char *canvas_id);
hu_error_t hu_canvas_persist_load_all(void *db, hu_canvas_store_t *store);

#endif
