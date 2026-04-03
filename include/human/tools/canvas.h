#ifndef HU_TOOLS_CANVAS_H
#define HU_TOOLS_CANVAS_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/tool.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

hu_error_t hu_canvas_tool_create(hu_allocator_t *alloc, hu_tool_t *out);

/**
 * Opaque handle for canvas state. Obtained from a canvas tool via
 * hu_canvas_store_from_tool(). Shared between the tool and RPC handlers.
 */
typedef struct hu_canvas_store hu_canvas_store_t;

hu_canvas_store_t *hu_canvas_store_from_tool(hu_tool_t *tool);

/** Snapshot of a single canvas for RPC responses. Strings are borrowed. */
typedef struct {
    const char *canvas_id;
    const char *title;
    const char *format;
    const char *content;
    const char *language;
    const char *imports;
    uint32_t version_seq;
    size_t version_count;
    bool user_edit_pending;
    const char *user_edited_content;
} hu_canvas_info_t;

size_t hu_canvas_store_count(const hu_canvas_store_t *store);
bool hu_canvas_store_get(const hu_canvas_store_t *store, size_t index, hu_canvas_info_t *info);
bool hu_canvas_store_find(const hu_canvas_store_t *store, const char *canvas_id,
                          hu_canvas_info_t *info);

hu_error_t hu_canvas_store_edit(hu_canvas_store_t *store, const char *canvas_id,
                                const char *new_content);
hu_error_t hu_canvas_store_undo(hu_canvas_store_t *store, const char *canvas_id,
                                hu_canvas_info_t *info);
hu_error_t hu_canvas_store_redo(hu_canvas_store_t *store, const char *canvas_id,
                                hu_canvas_info_t *info);

#endif
