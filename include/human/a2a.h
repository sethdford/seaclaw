#ifndef HU_A2A_H
#define HU_A2A_H
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct hu_a2a_skill { char *id; char *name; char *description; } hu_a2a_skill_t;
typedef struct hu_a2a_agent_card {
    char *name; char *description; char *url; char *version;
    hu_a2a_skill_t *skills; size_t skills_count;
} hu_a2a_agent_card_t;
typedef enum { HU_A2A_TASK_SUBMITTED=0, HU_A2A_TASK_WORKING, HU_A2A_TASK_INPUT_REQUIRED, HU_A2A_TASK_COMPLETED, HU_A2A_TASK_FAILED, HU_A2A_TASK_CANCELED } hu_a2a_task_state_t;
typedef struct hu_a2a_part { char *type; char *content; size_t content_len; } hu_a2a_part_t;
typedef struct hu_a2a_message { char *role; hu_a2a_part_t *parts; size_t parts_count; } hu_a2a_message_t;
typedef struct hu_a2a_task { char *id; hu_a2a_task_state_t state; hu_a2a_message_t *messages; size_t messages_count; } hu_a2a_task_t;

hu_error_t hu_a2a_discover(hu_allocator_t *alloc, const char *agent_url, hu_a2a_agent_card_t *out);
hu_error_t hu_a2a_send_task(hu_allocator_t *alloc, const char *agent_url, const hu_a2a_message_t *message, hu_a2a_task_t *out);
hu_error_t hu_a2a_get_task(hu_allocator_t *alloc, const char *agent_url, const char *task_id, hu_a2a_task_t *out);
hu_error_t hu_a2a_cancel_task(hu_allocator_t *alloc, const char *agent_url, const char *task_id);
hu_error_t hu_a2a_server_init(hu_allocator_t *alloc, const hu_a2a_agent_card_t *card);
hu_error_t hu_a2a_server_handle_request(hu_allocator_t *alloc, const char *method, const char *body, size_t body_len, char **response, size_t *response_len);
void hu_a2a_server_deinit(hu_allocator_t *alloc);
void hu_a2a_agent_card_free(hu_allocator_t *alloc, hu_a2a_agent_card_t *card);
void hu_a2a_task_free(hu_allocator_t *alloc, hu_a2a_task_t *task);
void hu_a2a_message_free(hu_allocator_t *alloc, hu_a2a_message_t *msg);
#endif
