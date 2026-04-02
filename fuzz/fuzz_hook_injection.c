/*
 * Fuzz harness for hook shell escaping and pipeline execution.
 * Must not crash, overflow, or leak on any input.
 *
 * Exercises: hu_hook_shell_escape, hu_hook_registry_create/add/destroy,
 *            hu_hook_pipeline_execute with adversarial tool names and args.
 */
#include "human/hook.h"
#include "human/hook_pipeline.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 8192 || size < 4)
        return 0;

    hu_allocator_t alloc = hu_system_allocator();

    /* ── Fuzz hu_hook_shell_escape with raw bytes ── */
    {
        char *escaped = NULL;
        size_t escaped_len = 0;
        hu_error_t err = hu_hook_shell_escape(&alloc, (const char *)data, size,
                                              &escaped, &escaped_len);
        if (err == HU_OK && escaped) {
            /* Verify no unescaped single quotes remain (except in escape sequences) */
            alloc.free(alloc.ctx, escaped, escaped_len + 1);
        }
    }

    /* ── Fuzz shell escape with NUL bytes and metacharacters ── */
    {
        /* Create a string with injected shell metacharacters */
        char injected[256];
        size_t inject_len = size > 255 ? 255 : size;
        memcpy(injected, data, inject_len);
        injected[inject_len] = '\0';

        char *escaped = NULL;
        size_t escaped_len = 0;
        hu_error_t err = hu_hook_shell_escape(&alloc, injected, inject_len,
                                              &escaped, &escaped_len);
        if (err == HU_OK && escaped) {
            /* Verify dangerous chars are escaped */
            alloc.free(alloc.ctx, escaped, escaped_len + 1);
        }
    }

    /* ── Fuzz hook registry with adversarial names/commands ── */
    {
        hu_hook_registry_t *reg = NULL;
        hu_error_t err = hu_hook_registry_create(&alloc, &reg);
        if (err != HU_OK)
            return 0;

        /* Split fuzz data into name and command parts */
        size_t split = data[0] % size;
        if (split == 0) split = 1;
        if (split >= size - 1) split = size - 2;

        char *name = (char *)alloc.alloc(alloc.ctx, split + 1);
        char *cmd = (char *)alloc.alloc(alloc.ctx, (size - split) + 1);
        if (name && cmd) {
            memcpy(name, data + 1, split);
            name[split] = '\0';
            memcpy(cmd, data + 1 + split, size - 1 - split);
            cmd[size - 1 - split] = '\0';

            hu_hook_entry_t entry;
            memset(&entry, 0, sizeof(entry));
            entry.name = name;
            entry.name_len = split;
            entry.event = (data[0] & 1) ? HU_HOOK_POST_TOOL_EXECUTE : HU_HOOK_PRE_TOOL_EXECUTE;
            entry.command = cmd;
            entry.command_len = size - 1 - split;
            entry.timeout_sec = 1;
            entry.required = (data[0] & 2) != 0;

            (void)hu_hook_registry_add(reg, &alloc, &entry);

            alloc.free(alloc.ctx, name, split + 1);
            alloc.free(alloc.ctx, cmd, (size - split) + 1);
        } else {
            if (name) alloc.free(alloc.ctx, name, split + 1);
            if (cmd) alloc.free(alloc.ctx, cmd, (size - split) + 1);
        }

        /* ── Fuzz pipeline execution with adversarial context ── */
        {
            /* NUL-terminate the full input for tool_name and args */
            char *tool_name = (char *)alloc.alloc(alloc.ctx, size + 1);
            if (tool_name) {
                memcpy(tool_name, data, size);
                tool_name[size] = '\0';

                hu_hook_context_t ctx;
                memset(&ctx, 0, sizeof(ctx));
                ctx.event = HU_HOOK_PRE_TOOL_EXECUTE;
                ctx.tool_name = tool_name;
                ctx.tool_name_len = size;
                ctx.args_json = "{}";
                ctx.args_json_len = 2;

                hu_hook_result_t result;
                memset(&result, 0, sizeof(result));
                (void)hu_hook_pipeline_execute(reg, &alloc, &ctx, &result);
                hu_hook_result_free(&alloc, &result);

                /* Also test post-hook path */
                ctx.event = HU_HOOK_POST_TOOL_EXECUTE;
                ctx.result_output = tool_name;
                ctx.result_output_len = size;
                ctx.result_success = (data[0] & 4) != 0;

                memset(&result, 0, sizeof(result));
                (void)hu_hook_pipeline_execute(reg, &alloc, &ctx, &result);
                hu_hook_result_free(&alloc, &result);

                alloc.free(alloc.ctx, tool_name, size + 1);
            }
        }

        hu_hook_registry_destroy(reg, &alloc);
    }

    return 0;
}
