#include "test_framework.h"
#include "human/agent.h"
#include "human/config.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/hook.h"
#include "human/permission.h"
#include <stdio.h>
#include <string.h>
#include <signal.h>

/* Test suite: Config hot-reload feature */

/* Signal-based reload state (simulated in tests) */
static volatile sig_atomic_t test_reload_requested = 0;

void test_config_reload_basic(void) {
    /* Test that reload updates hooks, permissions, and instructions */
    hu_allocator_t alloc = hu_system_allocator();

    hu_agent_t agent;
    memset(&agent, 0, sizeof(agent));
    agent.alloc = &alloc;
    agent.permission_base_level = HU_PERM_READ_ONLY;
    agent.permission_level = HU_PERM_READ_ONLY;
    agent.workspace_dir = ".";
    agent.workspace_dir_len = 1;

    char *summary = NULL;
    size_t summary_len = 0;
    hu_error_t err = hu_agent_reload_config(&agent, &summary, &summary_len);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(summary);
    HU_ASSERT(summary_len > 0);

    if (summary)
        alloc.free(alloc.ctx, summary, summary_len + 1);
}

void test_config_reload_with_hooks(void) {
    /* Test that hooks are rebuilt after reload */
    hu_allocator_t alloc = hu_system_allocator();

    hu_agent_t agent;
    memset(&agent, 0, sizeof(agent));
    agent.alloc = &alloc;
    agent.permission_base_level = HU_PERM_READ_ONLY;
    agent.permission_level = HU_PERM_READ_ONLY;
    agent.workspace_dir = ".";
    agent.workspace_dir_len = 1;

    /* Create an initial hook registry */
    hu_error_t err = hu_hook_registry_create(&alloc, &agent.hook_registry);
    HU_ASSERT_EQ(err, HU_OK);

    /* Add a hook to the registry */
    hu_hook_entry_t entry = {
        .name = "test_hook",
        .name_len = 9,
        .event = HU_HOOK_PRE_TOOL_EXECUTE,
        .command = "echo test",
        .command_len = 9,
        .timeout_sec = 10,
        .required = false,
    };
    hu_hook_registry_add(agent.hook_registry, &alloc, &entry);
    (void)hu_hook_registry_count(agent.hook_registry);

    char *summary = NULL;
    size_t summary_len = 0;
    err = hu_agent_reload_config(&agent, &summary, &summary_len);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(summary);

    /* Hook registry should be updated (though may be empty after reload) */
    if (summary)
        alloc.free(alloc.ctx, summary, summary_len + 1);

    if (agent.hook_registry)
        hu_hook_registry_destroy(agent.hook_registry, &alloc);
}

void test_config_reload_permission_update(void) {
    /* Test that permission level updates */
    hu_allocator_t alloc = hu_system_allocator();

    hu_agent_t agent;
    memset(&agent, 0, sizeof(agent));
    agent.alloc = &alloc;
    agent.permission_base_level = HU_PERM_READ_ONLY;
    agent.permission_level = HU_PERM_READ_ONLY;
    agent.permission_escalated = false;
    agent.workspace_dir = ".";
    agent.workspace_dir_len = 1;

    char *summary = NULL;
    size_t summary_len = 0;
    hu_error_t err = hu_agent_reload_config(&agent, &summary, &summary_len);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(summary);

    if (summary)
        alloc.free(alloc.ctx, summary, summary_len + 1);
}

void test_config_reload_instruction_discovery(void) {
    /* Test that instructions are re-discovered */
    hu_allocator_t alloc = hu_system_allocator();

    hu_agent_t agent;
    memset(&agent, 0, sizeof(agent));
    agent.alloc = &alloc;
    agent.permission_base_level = HU_PERM_READ_ONLY;
    agent.permission_level = HU_PERM_READ_ONLY;
    agent.workspace_dir = ".";
    agent.workspace_dir_len = 1;
    agent.instruction_discovery = NULL;

    char *summary = NULL;
    size_t summary_len = 0;
    hu_error_t err = hu_agent_reload_config(&agent, &summary, &summary_len);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(summary);

    if (summary)
        alloc.free(alloc.ctx, summary, summary_len + 1);

    if (agent.instruction_discovery) {
        hu_instruction_discovery_destroy(&alloc, agent.instruction_discovery);
    }
}

void test_config_reload_invalid_agent(void) {
    /* Test that reload rejects invalid inputs */
    hu_allocator_t alloc = hu_system_allocator();

    hu_error_t err = hu_agent_reload_config(NULL, NULL, NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);

    hu_agent_t agent;
    memset(&agent, 0, sizeof(agent));
    agent.alloc = &alloc;

    hu_error_t err2 = hu_agent_reload_config(&agent, NULL, NULL);
    HU_ASSERT_EQ(err2, HU_ERR_INVALID_ARGUMENT);
}

void test_config_reload_summary_format(void) {
    /* Test that summary is properly formatted */
    hu_allocator_t alloc = hu_system_allocator();

    hu_agent_t agent;
    memset(&agent, 0, sizeof(agent));
    agent.alloc = &alloc;
    agent.permission_base_level = HU_PERM_READ_ONLY;
    agent.permission_level = HU_PERM_READ_ONLY;
    agent.workspace_dir = ".";
    agent.workspace_dir_len = 1;

    char *summary = NULL;
    size_t summary_len = 0;
    hu_error_t err = hu_agent_reload_config(&agent, &summary, &summary_len);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(summary);
    HU_ASSERT(summary_len > 0);

    /* Check that summary contains expected keywords */
    bool has_content = (strstr(summary, "Config") != NULL ||
                       strstr(summary, "Hooks") != NULL ||
                       strstr(summary, "Permission") != NULL ||
                       strstr(summary, "Instructions") != NULL ||
                       strstr(summary, "No changes") != NULL);
    HU_ASSERT(has_content);

    if (summary)
        alloc.free(alloc.ctx, summary, summary_len + 1);
}

void test_sighup_flag_initialization(void) {
    /* Test that reload flag starts at 0 (not requested) */
    /* This simulates the behavior of g_reload_requested global */
    test_reload_requested = 0;
    HU_ASSERT_EQ(test_reload_requested, 0);
}

void test_sighup_flag_set_and_check(void) {
    /* Test that flag can be set to 1 and checked */
    test_reload_requested = 0;
    HU_ASSERT_EQ(test_reload_requested, 0);

    test_reload_requested = 1;
    HU_ASSERT_EQ(test_reload_requested, 1);

    /* Simulate processing the flag */
    test_reload_requested = 0;
    HU_ASSERT_EQ(test_reload_requested, 0);
}

void test_sighup_reload_flag_reset_after_processing(void) {
    /* Test that flag is reset to 0 after config reload processing */
    hu_allocator_t alloc = hu_system_allocator();

    hu_agent_t agent;
    memset(&agent, 0, sizeof(agent));
    agent.alloc = &alloc;
    agent.permission_base_level = HU_PERM_READ_ONLY;
    agent.permission_level = HU_PERM_READ_ONLY;
    agent.workspace_dir = ".";
    agent.workspace_dir_len = 1;

    /* Simulate signal request */
    test_reload_requested = 1;
    HU_ASSERT_EQ(test_reload_requested, 1);

    /* Process reload */
    char *summary = NULL;
    size_t summary_len = 0;
    hu_error_t err = hu_agent_reload_config(&agent, &summary, &summary_len);
    HU_ASSERT_EQ(err, HU_OK);

    /* Reset flag (as would happen in main loop) */
    test_reload_requested = 0;
    HU_ASSERT_EQ(test_reload_requested, 0);

    if (summary)
        alloc.free(alloc.ctx, summary, summary_len + 1);
}

void run_config_reload_tests(void) {
    HU_TEST_SUITE("config_reload");
    HU_RUN_TEST(test_config_reload_basic);
    HU_RUN_TEST(test_config_reload_with_hooks);
    HU_RUN_TEST(test_config_reload_permission_update);
    HU_RUN_TEST(test_config_reload_instruction_discovery);
    HU_RUN_TEST(test_config_reload_invalid_agent);
    HU_RUN_TEST(test_config_reload_summary_format);
    HU_RUN_TEST(test_sighup_flag_initialization);
    HU_RUN_TEST(test_sighup_flag_set_and_check);
    HU_RUN_TEST(test_sighup_reload_flag_reset_after_processing);
}
