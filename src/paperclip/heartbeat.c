#include "human/core/log.h"
#include "human/bootstrap.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/json.h"
#include "human/paperclip/client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t build_task_context(char *buf, size_t cap, const hu_paperclip_task_t *task,
                                 const hu_paperclip_comment_list_t *comments,
                                 const hu_paperclip_client_t *client) {
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
            const hu_paperclip_comment_t *c = &comments->comments[i];
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

hu_error_t hu_paperclip_heartbeat(hu_allocator_t *alloc, int argc, char **argv) {
    (void)argc;
    (void)argv;

    hu_paperclip_client_t client = {0};
    hu_error_t err = hu_paperclip_client_init(&client, alloc);
    if (err != HU_OK) {
        hu_log_error("paperclip", NULL, "Failed to init client. Set PAPERCLIP_API_URL "
                        "and PAPERCLIP_AGENT_ID.");
        return err;
    }

    hu_log_info("paperclip", NULL, "Heartbeat: agent=%s wake=%s", client.agent_id,
            client.wake_reason ? client.wake_reason : "timer");

    const char *target_task_id = client.task_id;
    hu_paperclip_task_t task = {0};
    hu_paperclip_task_list_t task_list = {0};

    if (target_task_id) {
        err = hu_paperclip_get_task(&client, target_task_id, &task);
        if (err != HU_OK) {
            hu_log_error("paperclip", NULL, "Failed to get task %s: %s", target_task_id,
                    hu_error_string(err));
            hu_paperclip_client_deinit(&client);
            return err;
        }
    } else {
        err = hu_paperclip_list_tasks(&client, &task_list);
        if (err != HU_OK) {
            hu_log_error("paperclip", NULL, "Failed to list tasks: %s", hu_error_string(err));
            hu_paperclip_client_deinit(&client);
            return err;
        }
        if (task_list.count == 0) {
            hu_log_info("paperclip", NULL, "No assigned tasks. Heartbeat complete.");
            hu_paperclip_task_list_free(alloc, &task_list);
            hu_paperclip_client_deinit(&client);
            return HU_OK;
        }
        task = task_list.tasks[0];
        target_task_id = task.id;
        memset(&task_list.tasks[0], 0, sizeof(hu_paperclip_task_t));
    }

    hu_log_info("paperclip", NULL, "Working on: %s — %s", target_task_id,
            task.title ? task.title : "(untitled)");

    err = hu_paperclip_checkout_task(&client, target_task_id);
    if (err == HU_ERR_ALREADY_EXISTS) {
        hu_log_info("paperclip", NULL, "Task already checked out by another agent.");
        hu_paperclip_task_free(alloc, &task);
        hu_paperclip_task_list_free(alloc, &task_list);
        hu_paperclip_client_deinit(&client);
        return HU_OK;
    }
    if (err != HU_OK) {
        hu_log_error("paperclip", NULL, "Checkout failed: %s", hu_error_string(err));
        hu_paperclip_task_free(alloc, &task);
        hu_paperclip_task_list_free(alloc, &task_list);
        hu_paperclip_client_deinit(&client);
        return err;
    }

    hu_paperclip_comment_list_t comments = {0};
    hu_paperclip_get_comments(&client, target_task_id, &comments);

    char context[4096];
    size_t ctx_len = build_task_context(context, sizeof(context), &task, &comments, &client);

    hu_log_info("paperclip", NULL, "Context built (%zu chars). Bootstrapping agent...", ctx_len);

    hu_app_ctx_t app = {0};
    const char *config_path = getenv("HUMAN_CONFIG_PATH");
    err = hu_app_bootstrap(&app, alloc, config_path, true, false);
    if (err != HU_OK || !app.agent_ok) {
        hu_log_error("paperclip", NULL, "Agent bootstrap failed: %s (agent_ok=%d)",
                hu_error_string(err), app.agent_ok);
        if (app.agent_ok)
            hu_app_teardown(&app);
        hu_paperclip_comment_list_free(alloc, &comments);
        hu_paperclip_task_free(alloc, &task);
        hu_paperclip_task_list_free(alloc, &task_list);
        hu_paperclip_client_deinit(&client);
        return err != HU_OK ? err : HU_ERR_NOT_SUPPORTED;
    }

    app.agent->conversation_context = context;
    app.agent->conversation_context_len = ctx_len;

    char *response = NULL;
    size_t response_len = 0;
    hu_log_info("paperclip", NULL, "Running agent turn...");
    err = hu_agent_turn(app.agent, context, ctx_len, &response, &response_len);

    if (err == HU_OK && response && response_len > 0) {
        hu_log_info("paperclip", NULL, "Agent responded (%zu chars). Posting comment...",
                response_len);
        hu_paperclip_post_comment(&client, target_task_id, response, response_len);

        bool mark_done = true;
        if (strstr(response, "BLOCKED") || strstr(response, "blocked"))
            mark_done = false;
        if (strstr(response, "IN_PROGRESS") || strstr(response, "in progress"))
            mark_done = false;

        const char *new_status = mark_done ? "done" : "in_progress";
        hu_paperclip_update_task(&client, target_task_id, new_status);
        hu_log_info("paperclip", NULL, "Task updated to '%s'", new_status);
    } else {
        hu_log_error("paperclip", NULL, "Agent turn failed: %s", hu_error_string(err));
        hu_paperclip_post_comment(&client, target_task_id,
                                  "Agent encountered an error processing this task.",
                                  strlen("Agent encountered an error processing this task."));
    }

    if (response)
        alloc->free(alloc->ctx, response, response_len + 1);

    hu_app_teardown(&app);

    hu_paperclip_comment_list_free(alloc, &comments);
    hu_paperclip_task_free(alloc, &task);
    hu_paperclip_task_list_free(alloc, &task_list);
    hu_paperclip_client_deinit(&client);

    hu_log_info("paperclip", NULL, "Heartbeat complete.");
    return HU_OK;
}
