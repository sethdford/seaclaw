#include "seaclaw/bootstrap.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/core/json.h"
#include "seaclaw/paperclip/client.h"
#include <stdio.h>
#include <string.h>

static size_t build_task_context(char *buf, size_t cap, const sc_paperclip_task_t *task,
                                 const sc_paperclip_comment_list_t *comments,
                                 const sc_paperclip_client_t *client) {
    size_t pos = 0;
    int w;

#define CTX_APPEND(...)                                      \
    do {                                                     \
        if (pos < cap) {                                     \
            w = snprintf(buf + pos, cap - pos, __VA_ARGS__); \
            if (w > 0 && pos + (size_t)w < cap)              \
                pos += (size_t)w;                            \
        }                                                    \
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
            CTX_APPEND("  [%s] %s: %s\n", c->created_at ? c->created_at : "?",
                       c->author_name ? c->author_name : "unknown", c->body ? c->body : "");
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

    fprintf(stderr, "[paperclip] Heartbeat: agent=%s wake=%s\n", client.agent_id,
            client.wake_reason ? client.wake_reason : "timer");

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

    fprintf(stderr, "[paperclip] Working on: %s — %s\n", target_task_id,
            task.title ? task.title : "(untitled)");

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

    char context[4096];
    size_t ctx_len = build_task_context(context, sizeof(context), &task, &comments, &client);

    fprintf(stderr, "[paperclip] Context built (%zu chars). Bootstrapping agent...\n", ctx_len);

    sc_app_ctx_t app = {0};
    err = sc_app_bootstrap(&app, alloc, NULL, true, false);
    if (err != SC_OK || !app.agent_ok) {
        fprintf(stderr, "[paperclip] Agent bootstrap failed: %d (agent_ok=%d)\n", (int)err,
                app.agent_ok);
        if (app.agent_ok)
            sc_app_teardown(&app);
        sc_paperclip_comment_list_free(alloc, &comments);
        sc_paperclip_task_free(alloc, &task);
        sc_paperclip_task_list_free(alloc, &task_list);
        sc_paperclip_client_deinit(&client);
        return err != SC_OK ? err : SC_ERR_NOT_SUPPORTED;
    }

    app.agent->conversation_context = context;
    app.agent->conversation_context_len = ctx_len;

    char *response = NULL;
    size_t response_len = 0;
    fprintf(stderr, "[paperclip] Running agent turn...\n");
    err = sc_agent_turn(app.agent, context, ctx_len, &response, &response_len);

    if (err == SC_OK && response && response_len > 0) {
        fprintf(stderr, "[paperclip] Agent responded (%zu chars). Posting comment...\n",
                response_len);
        sc_paperclip_post_comment(&client, target_task_id, response, response_len);

        bool mark_done = true;
        if (strstr(response, "BLOCKED") || strstr(response, "blocked"))
            mark_done = false;
        if (strstr(response, "IN_PROGRESS") || strstr(response, "in progress"))
            mark_done = false;

        const char *new_status = mark_done ? "done" : "in_progress";
        sc_paperclip_update_task(&client, target_task_id, new_status);
        fprintf(stderr, "[paperclip] Task updated to '%s'\n", new_status);
    } else {
        fprintf(stderr, "[paperclip] Agent turn failed: %d\n", (int)err);
        sc_paperclip_post_comment(&client, target_task_id,
                                  "Agent encountered an error processing this task.",
                                  strlen("Agent encountered an error processing this task."));
    }

    if (response)
        alloc->free(alloc->ctx, response, response_len + 1);

    sc_app_teardown(&app);

    sc_paperclip_comment_list_free(alloc, &comments);
    sc_paperclip_task_free(alloc, &task);
    sc_paperclip_task_list_free(alloc, &task_list);
    sc_paperclip_client_deinit(&client);

    fprintf(stderr, "[paperclip] Heartbeat complete.\n");
    return SC_OK;
}
