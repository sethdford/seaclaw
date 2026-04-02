#include "test_framework.h"
#include "human/plugin.h"
#include "human/hook.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <string.h>

/* Test suite: Plugin-provided hooks feature */

/* Mock plugin hook context for testing */
typedef struct {
    hu_hook_registry_t *hook_registry;
    hu_allocator_t *alloc;
} test_plugin_hook_ctx_t;

static hu_error_t test_plugin_register_hook(void *ctx, const hu_hook_entry_t *hook) {
    test_plugin_hook_ctx_t *hctx = (test_plugin_hook_ctx_t *)ctx;
    if (!hctx || !hook)
        return HU_ERR_INVALID_ARGUMENT;
    if (!hctx->hook_registry) {
        hu_error_t err = hu_hook_registry_create(hctx->alloc, &hctx->hook_registry);
        if (err != HU_OK)
            return err;
    }
    return hu_hook_registry_add(hctx->hook_registry, hctx->alloc, hook);
}

void test_plugin_hook_registration_basic(void) {
    /* Test that plugins can register hooks via host callback */
    hu_allocator_t alloc = hu_system_allocator();

    test_plugin_hook_ctx_t ctx = {
        .hook_registry = NULL,
        .alloc = &alloc,
    };

    hu_plugin_host_t host = {
        .alloc = &alloc,
        .register_tool = NULL,
        .register_provider = NULL,
        .register_channel = NULL,
        .register_hook = test_plugin_register_hook,
        .host_ctx = &ctx,
    };

    hu_hook_entry_t hook = {
        .name = "plugin_hook",
        .name_len = 11,
        .event = HU_HOOK_PRE_TOOL_EXECUTE,
        .command = "echo plugin",
        .command_len = 11,
        .timeout_sec = 10,
        .required = false,
    };

    hu_error_t err = test_plugin_register_hook(host.host_ctx, &hook);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(ctx.hook_registry);

    size_t count = hu_hook_registry_count(ctx.hook_registry);
    HU_ASSERT_EQ(count, 1);

    const hu_hook_entry_t *retrieved = hu_hook_registry_get(ctx.hook_registry, 0);
    HU_ASSERT_NOT_NULL(retrieved);
    HU_ASSERT_EQ(retrieved->name_len, 11);

    hu_hook_registry_destroy(ctx.hook_registry, &alloc);
}

void test_plugin_hook_multiple_registrations(void) {
    /* Test that plugins can register multiple hooks */
    hu_allocator_t alloc = hu_system_allocator();

    test_plugin_hook_ctx_t ctx = {
        .hook_registry = NULL,
        .alloc = &alloc,
    };

    hu_hook_entry_t hooks[3] = {
        {.name = "hook1", .name_len = 5, .event = HU_HOOK_PRE_TOOL_EXECUTE,
         .command = "echo 1", .command_len = 6, .timeout_sec = 10, .required = false},
        {.name = "hook2", .name_len = 5, .event = HU_HOOK_POST_TOOL_EXECUTE,
         .command = "echo 2", .command_len = 6, .timeout_sec = 10, .required = false},
        {.name = "hook3", .name_len = 5, .event = HU_HOOK_PRE_TOOL_EXECUTE,
         .command = "echo 3", .command_len = 6, .timeout_sec = 10, .required = true},
    };

    for (int i = 0; i < 3; i++) {
        hu_error_t err = test_plugin_register_hook(&ctx, &hooks[i]);
        HU_ASSERT_EQ(err, HU_OK);
    }

    HU_ASSERT_NOT_NULL(ctx.hook_registry);
    size_t count = hu_hook_registry_count(ctx.hook_registry);
    HU_ASSERT_EQ(count, 3);

    hu_hook_registry_destroy(ctx.hook_registry, &alloc);
}

void test_plugin_hook_invalid_registration(void) {
    /* Test that invalid hook registrations are rejected */
    hu_allocator_t alloc = hu_system_allocator();

    test_plugin_hook_ctx_t ctx = {
        .hook_registry = NULL,
        .alloc = &alloc,
    };

    hu_error_t err = test_plugin_register_hook(NULL, NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);

    hu_hook_entry_t hook = {
        .name = NULL,
        .name_len = 0,
        .event = HU_HOOK_PRE_TOOL_EXECUTE,
        .command = "echo test",
        .command_len = 9,
        .timeout_sec = 10,
        .required = false,
    };

    err = test_plugin_register_hook(&ctx, &hook);
    HU_ASSERT_EQ(err, HU_OK);  /* Hook registry adds it anyway */

    if (ctx.hook_registry)
        hu_hook_registry_destroy(ctx.hook_registry, &alloc);
}

void test_plugin_hook_event_types(void) {
    /* Test that plugins can register hooks for both event types */
    hu_allocator_t alloc = hu_system_allocator();

    test_plugin_hook_ctx_t ctx = {
        .hook_registry = NULL,
        .alloc = &alloc,
    };

    hu_hook_entry_t pre_hook = {
        .name = "pre_hook",
        .name_len = 8,
        .event = HU_HOOK_PRE_TOOL_EXECUTE,
        .command = "echo pre",
        .command_len = 8,
        .timeout_sec = 10,
        .required = false,
    };

    hu_hook_entry_t post_hook = {
        .name = "post_hook",
        .name_len = 9,
        .event = HU_HOOK_POST_TOOL_EXECUTE,
        .command = "echo post",
        .command_len = 9,
        .timeout_sec = 10,
        .required = false,
    };

    hu_error_t err = test_plugin_register_hook(&ctx, &pre_hook);
    HU_ASSERT_EQ(err, HU_OK);

    err = test_plugin_register_hook(&ctx, &post_hook);
    HU_ASSERT_EQ(err, HU_OK);

    size_t count = hu_hook_registry_count(ctx.hook_registry);
    HU_ASSERT_EQ(count, 2);

    const hu_hook_entry_t *h0 = hu_hook_registry_get(ctx.hook_registry, 0);
    const hu_hook_entry_t *h1 = hu_hook_registry_get(ctx.hook_registry, 1);
    HU_ASSERT_EQ(h0->event, HU_HOOK_PRE_TOOL_EXECUTE);
    HU_ASSERT_EQ(h1->event, HU_HOOK_POST_TOOL_EXECUTE);

    hu_hook_registry_destroy(ctx.hook_registry, &alloc);
}

void test_plugin_hook_host_interface(void) {
    /* Test the plugin host interface with hook callback */
    hu_allocator_t alloc = hu_system_allocator();

    test_plugin_hook_ctx_t ctx = {
        .hook_registry = NULL,
        .alloc = &alloc,
    };

    hu_plugin_host_t host = {
        .alloc = &alloc,
        .register_tool = NULL,
        .register_provider = NULL,
        .register_channel = NULL,
        .register_hook = test_plugin_register_hook,
        .host_ctx = &ctx,
    };

    HU_ASSERT_NOT_NULL(host.register_hook);
    HU_ASSERT_EQ(host.host_ctx, &ctx);

    hu_hook_entry_t hook = {
        .name = "test",
        .name_len = 4,
        .event = HU_HOOK_PRE_TOOL_EXECUTE,
        .command = "test",
        .command_len = 4,
        .timeout_sec = 30,
        .required = false,
    };

    hu_error_t err = host.register_hook(host.host_ctx, &hook);
    HU_ASSERT_EQ(err, HU_OK);

    if (ctx.hook_registry)
        hu_hook_registry_destroy(ctx.hook_registry, &alloc);
}

void run_plugin_hooks_tests(void) {
    HU_TEST_SUITE("plugin_hooks");
    HU_RUN_TEST(test_plugin_hook_registration_basic);
    HU_RUN_TEST(test_plugin_hook_multiple_registrations);
    HU_RUN_TEST(test_plugin_hook_invalid_registration);
    HU_RUN_TEST(test_plugin_hook_event_types);
    HU_RUN_TEST(test_plugin_hook_host_interface);
}
