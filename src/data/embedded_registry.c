#include <stddef.h>
#include <string.h>

/* Forward declarations for embedded data arrays */
extern const unsigned char data_prompts_group_chat_hint_txt[];
extern const size_t data_prompts_group_chat_hint_txt_len;

typedef struct {
    const char *path;
    const unsigned char *data;
    size_t len;
} hu_embedded_data_entry_t;

/* Helper macro to get length at runtime */
#define HU_EMBEDDED_ENTRY(path_str, var) \
    { .path = (path_str), .data = (var), .len = 0 }

static hu_embedded_data_entry_t hu_embedded_data_registry[] = {
    HU_EMBEDDED_ENTRY("prompts/group_chat_hint.txt", data_prompts_group_chat_hint_txt),
    { .path = NULL, .data = NULL, .len = 0 }  /* Sentinel */
};

static const size_t hu_embedded_data_count = 1;  /* excluding sentinel */

const hu_embedded_data_entry_t *hu_embedded_data_lookup(const char *path) {
    if (path == NULL)
        return NULL;

    for (size_t i = 0; i < hu_embedded_data_count; i++) {
        if (strcmp(hu_embedded_data_registry[i].path, path) == 0) {
            /* Set the length from the associated extern variable */
            if (strcmp(path, "prompts/group_chat_hint.txt") == 0) {
                hu_embedded_data_registry[i].len = data_prompts_group_chat_hint_txt_len;
            }
            return &hu_embedded_data_registry[i];
        }
    }

    return NULL;
}
