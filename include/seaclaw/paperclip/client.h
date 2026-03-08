#ifndef SC_PAPERCLIP_CLIENT_H
#define SC_PAPERCLIP_CLIENT_H

#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include <stddef.h>
#include <stdint.h>

typedef struct sc_paperclip_client {
    sc_allocator_t *alloc;
    char *api_url;
    char *agent_id;
    char *company_id;
    char *api_key;
    char *run_id;
    char *task_id;
    char *wake_reason;
} sc_paperclip_client_t;

typedef struct sc_paperclip_task {
    char *id;
    char *title;
    char *description;
    char *status;
    char *priority;
    char *project_name;
    char *goal_title;
} sc_paperclip_task_t;

typedef struct sc_paperclip_task_list {
    sc_paperclip_task_t *tasks;
    size_t count;
} sc_paperclip_task_list_t;

typedef struct sc_paperclip_comment {
    char *id;
    char *body;
    char *author_name;
    char *created_at;
} sc_paperclip_comment_t;

typedef struct sc_paperclip_comment_list {
    sc_paperclip_comment_t *comments;
    size_t count;
} sc_paperclip_comment_list_t;

sc_error_t sc_paperclip_client_init(sc_paperclip_client_t *client, sc_allocator_t *alloc);
sc_error_t sc_paperclip_client_init_from_config(sc_paperclip_client_t *client,
                                                 sc_allocator_t *alloc, const char *api_url,
                                                 const char *agent_id, const char *company_id);
void sc_paperclip_client_deinit(sc_paperclip_client_t *client);

sc_error_t sc_paperclip_list_tasks(sc_paperclip_client_t *client,
                                    sc_paperclip_task_list_t *out);
sc_error_t sc_paperclip_get_task(sc_paperclip_client_t *client, const char *task_id,
                                  sc_paperclip_task_t *out);
sc_error_t sc_paperclip_checkout_task(sc_paperclip_client_t *client, const char *task_id);
sc_error_t sc_paperclip_update_task(sc_paperclip_client_t *client, const char *task_id,
                                     const char *status);
sc_error_t sc_paperclip_post_comment(sc_paperclip_client_t *client, const char *task_id,
                                      const char *body, size_t body_len);
sc_error_t sc_paperclip_get_comments(sc_paperclip_client_t *client, const char *task_id,
                                      sc_paperclip_comment_list_t *out);

void sc_paperclip_task_free(sc_allocator_t *alloc, sc_paperclip_task_t *task);
void sc_paperclip_task_list_free(sc_allocator_t *alloc, sc_paperclip_task_list_t *list);
void sc_paperclip_comment_list_free(sc_allocator_t *alloc, sc_paperclip_comment_list_t *list);

#endif /* SC_PAPERCLIP_CLIENT_H */
