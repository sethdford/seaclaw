#include "seaclaw/paperclip/client.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/core/json.h"
#include <stdio.h>
#include <string.h>

#ifdef SC_HAS_PERSONA
#include "seaclaw/agent.h"
#include "seaclaw/config.h"
#include "seaclaw/persona.h"
#include "seaclaw/provider.h"
#include "seaclaw/tool.h"

__attribute__((unused))
static size_t build_task_context(char *buf, size_t cap,
                                  const sc_paperclip_task_t *task,
                                  const sc_paperclip_comment_list_t *comments,
                                  const sc_paperclip_client_t *client) {
    size_t pos = 0;
    int w;

#define CTX_APPEND(...)                                                                            \
    do {                                                                                           \
        if (pos < cap) {                                                                           \
            w = snprintf(buf + pos, cap - pos, __VA_ARGS__);                                       \
            if (w > 0 && pos + (size_t)w < cap)                                                    \
                pos += (size_t)w;                                                                  \
        }                                                                                          \
    } while (0)

    CTX_APPEND("PAPERCLIP TASK CONTEXT:\n");
    if (task->goal_title)
        CTX_APPEND("Goal: %s\n", task->goal_title);
    if (task->project_name)
        CTX_APPEND("Project: %s\n", task->project_name);
    CTX_APPEND("Task: %s\n", task->title ? task->title : "(untitled)");
    if (task->priority)
        CTX_APPEND("Priority: %s\n", task->priority);
    if (task->status)
        CTX_APPEND("Status: %s\n", task->status);
    if (task->description)
        CTX_APPEND("\nDescription:\n%s\n", task->description);

    if (comments && comments->count > 0) {
        CTX_APPEND("\nRecent comments:\n");
        size_t start = comments->count > 5 ? comments->count - 5 : 0;
        for (size_t i = start; i < comments->count; i++) {
            const sc_paperclip_comment_t *c = &comments->comments[i];
            CTX_APPEND("  [%s] %s: %s\n",
                       c->created_at ? c->created_at : "?",
                       c->author_name ? c->author_name : "unknown",
                       c->body ? c->body : "");
        }
    }

    if (client->wake_reason)
        CTX_APPEND("\nWake reason: %s\n", client->wake_reason);

    CTX_APPEND("\nYou have access to the paperclip tool to update task status and "
               "post comments. When done, use paperclip_update_status to mark the "
               "task complete.\n");

#undef CTX_APPEND
    return pos;
}
#endif /* SC_HAS_PERSONA */

sc_error_t sc_paperclip_heartbeat(sc_allocator_t *alloc, int argc, char **argv) {
    (void)argc;
    (void)argv;

    sc_paperclip_client_t client = {0};
    sc_error_t err = sc_paperclip_client_init(&client, alloc);
    if (err != SC_OK) {
        fprintf(stderr, "[paperclip] Failed to init client. Set PAPERCLIP_API_URL "
                        "and PAPERCLIP_AGENT_ID.\n");
        return err;
    }

    fprintf(stderr, "[paperclip] Heartbeat: agent=%s wake=%s\n",
            client.agent_id, client.wake_reason ? client.wake_reason : "timer");

    const char *target_task_id = client.task_id;
    sc_paperclip_task_t task = {0};
    sc_paperclip_task_list_t task_list = {0};

    if (target_task_id) {
        err = sc_paperclip_get_task(&client, target_task_id, &task);
        if (err != SC_OK) {
            fprintf(stderr, "[paperclip] Failed to get task %s: %d\n", target_task_id, (int)err);
            sc_paperclip_client_deinit(&client);
            return err;
        }
    } else {
        err = sc_paperclip_list_tasks(&client, &task_list);
        if (err != SC_OK) {
            fprintf(stderr, "[paperclip] Failed to list tasks: %d\n", (int)err);
            sc_paperclip_client_deinit(&client);
            return err;
        }
        if (task_list.count == 0) {
            fprintf(stderr, "[paperclip] No assigned tasks. Heartbeat complete.\n");
            sc_paperclip_task_list_free(alloc, &task_list);
            sc_paperclip_client_deinit(&client);
            return SC_OK;
        }
        task = task_list.tasks[0];
        target_task_id = task.id;
        memset(&task_list.tasks[0], 0, sizeof(sc_paperclip_task_t));
    }

    fprintf(stderr, "[paperclip] Working on: %s — %s\n",
            target_task_id, task.title ? task.title : "(untitled)");

    err = sc_paperclip_checkout_task(&client, target_task_id);
    if (err == SC_ERR_ALREADY_EXISTS) {
        fprintf(stderr, "[paperclip] Task already checked out by another agent.\n");
        sc_paperclip_task_free(alloc, &task);
        sc_paperclip_task_list_free(alloc, &task_list);
        sc_paperclip_client_deinit(&client);
        return SC_OK;
    }
    if (err != SC_OK) {
        fprintf(stderr, "[paperclip] Checkout failed: %d\n", (int)err);
        sc_paperclip_task_free(alloc, &task);
        sc_paperclip_task_list_free(alloc, &task_list);
        sc_paperclip_client_deinit(&client);
        return err;
    }

    sc_paperclip_comment_list_t comments = {0};
    sc_paperclip_get_comments(&client, target_task_id, &comments);

    /* TODO: re-wire when agent bootstrap API is stabilized */
    (void)comments;
    (void)err;
    fprintf(stderr, "[paperclip] agent integration pending API update\n");

    sc_paperclip_comment_list_free(alloc, &comments);
    sc_paperclip_task_free(alloc, &task);
    sc_paperclip_task_list_free(alloc, &task_list);
    sc_paperclip_client_deinit(&client);

    fprintf(stderr, "[paperclip] Heartbeat complete.\n");
    return SC_OK;
}
