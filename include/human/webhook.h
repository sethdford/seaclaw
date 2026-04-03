#ifndef HU_WEBHOOK_H
#define HU_WEBHOOK_H

#include "core/allocator.h"
#include "core/error.h"
#include <time.h>

typedef struct hu_webhook_manager hu_webhook_manager_t;

typedef struct {
    char *id;
    char *path;
    time_t registered_at;
} hu_webhook_t;

typedef struct {
    char *webhook_id;
    char *event_data;
    time_t received_at;
} hu_webhook_event_t;

hu_error_t hu_webhook_manager_create(hu_allocator_t *alloc, hu_webhook_manager_t **out);
void hu_webhook_manager_destroy(hu_allocator_t *alloc, hu_webhook_manager_t *mgr);

hu_error_t hu_webhook_register(hu_allocator_t *alloc, hu_webhook_manager_t *mgr, const char *path,
                               char **out_id);
hu_error_t hu_webhook_unregister(hu_allocator_t *alloc, hu_webhook_manager_t *mgr,
                                 const char *webhook_id);
hu_error_t hu_webhook_receive_event(hu_allocator_t *alloc, hu_webhook_manager_t *mgr,
                                    const char *webhook_id, const char *event_data);
hu_error_t hu_webhook_poll(hu_allocator_t *alloc, hu_webhook_manager_t *mgr,
                           const char *webhook_id, hu_webhook_event_t **out_events,
                           size_t *out_count);
hu_error_t hu_webhook_list(hu_allocator_t *alloc, hu_webhook_manager_t *mgr,
                           hu_webhook_t **out_webhooks, size_t *out_count);
void hu_webhook_free_webhooks(hu_allocator_t *alloc, hu_webhook_t *webhooks, size_t count);
void hu_webhook_free_events(hu_allocator_t *alloc, hu_webhook_event_t *events, size_t count);

#endif /* HU_WEBHOOK_H */
