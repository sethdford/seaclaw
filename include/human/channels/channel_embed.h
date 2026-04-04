#ifndef HU_CHANNEL_EMBED_H
#define HU_CHANNEL_EMBED_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>
#include <stdint.h>

#define HU_EMBED_MAX_BUTTONS 5

typedef enum hu_embed_type {
    HU_EMBED_RICH,
    HU_EMBED_IMAGE,
    HU_EMBED_ACTION
} hu_embed_type_t;

typedef struct hu_embed_button {
    char *label;
    char *url;
} hu_embed_button_t;

typedef struct hu_embed {
    hu_embed_type_t type;
    char *title;
    char *description;
    char *image_url;
    char *thumbnail_url;
    char *footer;
    uint32_t color;
    hu_embed_button_t buttons[HU_EMBED_MAX_BUTTONS];
    size_t button_count;
} hu_embed_t;

/* Format embed as Discord JSON (embed object + components). Caller frees *out. */
hu_error_t hu_embed_format_discord(hu_allocator_t *alloc, const hu_embed_t *embed,
                                   char **out, size_t *out_len);

/* Format embed as Slack Block Kit JSON. Caller frees *out. */
hu_error_t hu_embed_format_slack(hu_allocator_t *alloc, const hu_embed_t *embed,
                                 char **out, size_t *out_len);

/* Format embed as Telegram inline keyboard JSON. Caller frees *out. */
hu_error_t hu_embed_format_telegram(hu_allocator_t *alloc, const hu_embed_t *embed,
                                    char **out, size_t *out_len);

/* Free all strings within an embed (does NOT free the embed struct itself). */
void hu_embed_deinit(hu_embed_t *embed, hu_allocator_t *alloc);

#endif
