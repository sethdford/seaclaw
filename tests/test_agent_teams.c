/* Tests for worktree integration, team config, mailbox, and task list. */

#include "seaclaw/agent.h"
#include "seaclaw/agent/mailbox.h"
#include "seaclaw/agent/spawn.h"
#include "seaclaw/agent/task_list.h"
#include "seaclaw/agent/team.h"
#include "seaclaw/agent/worktree.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/providers/factory.h"
#include "test_framework.h"
#include <string.h>

static void test_worktree_create_generates_correct_path_and_branch(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_worktree_manager_t *mgr =
        sc_worktree_manager_create(&alloc, "/tmp/repo", 8);
    SC_ASSERT_NOT_NULL(mgr);

    sc_worktree_t wt = {0};
    sc_error_t err = sc_worktree_create(mgr, 42, &wt);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(wt.path);
    SC_ASSERT_NOT_NULL(wt.branch);
    SC_ASSERT_STR_EQ(wt.path, "/tmp/repo-agent-42");
    SC_ASSERT_STR_EQ(wt.branch, "agent/42");
    SC_ASSERT_EQ(wt.agent_id, 42u);
    SC_ASSERT_TRUE(wt.active);

    sc_worktree_free(&alloc, &wt);
    sc_worktree_manager_destroy(mgr);
}

static void test_worktree_remove_marks_inactive(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_worktree_manager_t *mgr =
        sc_worktree_manager_create(&alloc, "/tmp/repo", 8);
    SC_ASSERT_NOT_NULL(mgr);

    sc_worktree_t wt = {0};
    SC_ASSERT_EQ(sc_worktree_create(mgr, 1, &wt), SC_OK);
    sc_worktree_free(&alloc, &wt);

    SC_ASSERT_EQ(sc_worktree_remove(mgr, 1), SC_OK);

    sc_worktree_t out = {0};
    SC_ASSERT_EQ(sc_worktree_get(mgr, 1, &out), SC_ERR_NOT_FOUND);

    sc_worktree_manager_destroy(mgr);
}

static void test_worktree_list_returns_active_only(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_worktree_manager_t *mgr =
        sc_worktree_manager_create(&alloc, "/tmp/repo", 8);
    SC_ASSERT_NOT_NULL(mgr);

    sc_worktree_t wt1 = {0}, wt2 = {0};
    SC_ASSERT_EQ(sc_worktree_create(mgr, 1, &wt1), SC_OK);
    SC_ASSERT_EQ(sc_worktree_create(mgr, 2, &wt2), SC_OK);
    sc_worktree_free(&alloc, &wt1);
    sc_worktree_free(&alloc, &wt2);

    SC_ASSERT_EQ(sc_worktree_remove(mgr, 1), SC_OK);

    sc_worktree_t *list = NULL;
    size_t count = 0;
    SC_ASSERT_EQ(sc_worktree_list(mgr, &list, &count), SC_OK);
    SC_ASSERT_EQ(count, 1u);
    SC_ASSERT_NOT_NULL(list);
    SC_ASSERT_STR_EQ(list[0].path, "/tmp/repo-agent-2");
    SC_ASSERT_EQ(list[0].agent_id, 2u);

    for (size_t i = 0; i < count; i++)
        sc_worktree_free(&alloc, &list[i]);
    alloc.free(alloc.ctx, list, count * sizeof(sc_worktree_t));
    sc_worktree_manager_destroy(mgr);
}

static void test_worktree_get_finds_by_agent_id(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_worktree_manager_t *mgr =
        sc_worktree_manager_create(&alloc, "/tmp/repo", 8);
    SC_ASSERT_NOT_NULL(mgr);

    sc_worktree_t wt = {0};
    SC_ASSERT_EQ(sc_worktree_create(mgr, 99, &wt), SC_OK);
    sc_worktree_free(&alloc, &wt);

    sc_worktree_t out = {0};
    SC_ASSERT_EQ(sc_worktree_get(mgr, 99, &out), SC_OK);
    SC_ASSERT_STR_EQ(out.path, "/tmp/repo-agent-99");
    SC_ASSERT_STR_EQ(out.branch, "agent/99");
    SC_ASSERT_EQ(out.agent_id, 99u);

    sc_worktree_free(&alloc, &out);
    sc_worktree_manager_destroy(mgr);
}

static void test_team_config_parse_with_3_members(void) {
    sc_allocator_t alloc = sc_system_allocator();
    const char *json =
        "{\"name\":\"checkout-feature\",\"base_branch\":\"main\",\"members\":["
        "{\"name\":\"backend\",\"role\":\"builder\",\"autonomy\":\"assisted\","
        "\"model\":\"anthropic/claude-sonnet\",\"tools\":[\"shell\",\"file_write\"]},"
        "{\"name\":\"frontend\",\"role\":\"builder\",\"autonomy\":\"assisted\"},"
        "{\"name\":\"reviewer\",\"role\":\"reviewer\",\"autonomy\":\"locked\","
        "\"tools\":[\"file_read\"]}]}";
    sc_team_config_t cfg = {0};
    SC_ASSERT_EQ(sc_team_config_parse(&alloc, json, strlen(json), &cfg), SC_OK);
    SC_ASSERT_STR_EQ(cfg.name, "checkout-feature");
    SC_ASSERT_STR_EQ(cfg.base_branch, "main");
    SC_ASSERT_EQ(cfg.members_count, 3u);

    SC_ASSERT_STR_EQ(cfg.members[0].name, "backend");
    SC_ASSERT_STR_EQ(cfg.members[0].role, "builder");
    SC_ASSERT_EQ(cfg.members[0].autonomy, SC_AUTONOMY_ASSISTED);
    SC_ASSERT_STR_EQ(cfg.members[0].model, "anthropic/claude-sonnet");
    SC_ASSERT_EQ(cfg.members[0].allowed_tools_count, 2u);
    SC_ASSERT_STR_EQ(cfg.members[0].allowed_tools[0], "shell");
    SC_ASSERT_STR_EQ(cfg.members[0].allowed_tools[1], "file_write");

    sc_team_config_free(&alloc, &cfg);
}

static void test_team_config_get_member_by_name(void) {
    sc_allocator_t alloc = sc_system_allocator();
    const char *json =
        "{\"name\":\"team\",\"members\":["
        "{\"name\":\"backend\",\"role\":\"builder\"},"
        "{\"name\":\"reviewer\",\"role\":\"reviewer\"}]}";
    sc_team_config_t cfg = {0};
    SC_ASSERT_EQ(sc_team_config_parse(&alloc, json, strlen(json), &cfg), SC_OK);

    const sc_team_member_t *m = sc_team_config_get_member(&cfg, "reviewer");
    SC_ASSERT_NOT_NULL(m);
    SC_ASSERT_STR_EQ(m->name, "reviewer");
    SC_ASSERT_STR_EQ(m->role, "reviewer");

    SC_ASSERT_NULL(sc_team_config_get_member(&cfg, "nonexistent"));

    sc_team_config_free(&alloc, &cfg);
}

static void test_team_config_get_by_role_finds_first_match(void) {
    sc_allocator_t alloc = sc_system_allocator();
    const char *json =
        "{\"name\":\"team\",\"members\":["
        "{\"name\":\"backend\",\"role\":\"builder\"},"
        "{\"name\":\"frontend\",\"role\":\"builder\"},"
        "{\"name\":\"reviewer\",\"role\":\"reviewer\"}]}";
    sc_team_config_t cfg = {0};
    SC_ASSERT_EQ(sc_team_config_parse(&alloc, json, strlen(json), &cfg), SC_OK);

    const sc_team_member_t *m = sc_team_config_get_by_role(&cfg, "builder");
    SC_ASSERT_NOT_NULL(m);
    SC_ASSERT_STR_EQ(m->name, "backend");

    sc_team_config_free(&alloc, &cfg);
}

static void test_team_config_parse_autonomy_levels_maps_correctly(void) {
    sc_allocator_t alloc = sc_system_allocator();
    const char *json =
        "{\"name\":\"team\",\"members\":["
        "{\"name\":\"a\",\"autonomy\":\"locked\"},"
        "{\"name\":\"b\",\"autonomy\":\"supervised\"},"
        "{\"name\":\"c\",\"autonomy\":\"assisted\"},"
        "{\"name\":\"d\",\"autonomy\":\"autonomous\"}]}";
    sc_team_config_t cfg = {0};
    SC_ASSERT_EQ(sc_team_config_parse(&alloc, json, strlen(json), &cfg), SC_OK);

    SC_ASSERT_EQ(cfg.members[0].autonomy, SC_AUTONOMY_LOCKED);
    SC_ASSERT_EQ(cfg.members[1].autonomy, SC_AUTONOMY_SUPERVISED);
    SC_ASSERT_EQ(cfg.members[2].autonomy, SC_AUTONOMY_ASSISTED);
    SC_ASSERT_EQ(cfg.members[3].autonomy, SC_AUTONOMY_AUTONOMOUS);

    sc_team_config_free(&alloc, &cfg);
}

static void test_team_config_parse_missing_fields_uses_defaults(void) {
    sc_allocator_t alloc = sc_system_allocator();
    const char *json = "{\"members\":[{\"name\":\"x\"}]}";
    sc_team_config_t cfg = {0};
    SC_ASSERT_EQ(sc_team_config_parse(&alloc, json, strlen(json), &cfg), SC_OK);

    SC_ASSERT_NULL(cfg.name);
    SC_ASSERT_NULL(cfg.base_branch);
    SC_ASSERT_EQ(cfg.members_count, 1u);
    SC_ASSERT_STR_EQ(cfg.members[0].name, "x");
    SC_ASSERT_NULL(cfg.members[0].role);
    SC_ASSERT_EQ(cfg.members[0].autonomy, SC_AUTONOMY_ASSISTED);
    SC_ASSERT_NULL(cfg.members[0].allowed_tools);
    SC_ASSERT_EQ(cfg.members[0].allowed_tools_count, 0u);
    SC_ASSERT_NULL(cfg.members[0].model);

    sc_team_config_free(&alloc, &cfg);
}

/* ── Mailbox + agent integration ──────────────────────────────────────── */
static void test_mailbox_recv_in_agent_context_works(void) {
    sc_allocator_t a = sc_system_allocator();
    sc_mailbox_t *mb = sc_mailbox_create(&a, 8);
    SC_ASSERT_NOT_NULL(mb);

    sc_provider_t prov = {0};
    SC_ASSERT_EQ(sc_provider_create(&a, "openai", 6, "test-key", 8, "", 0, &prov), SC_OK);

    sc_agent_t agent = {0};
    SC_ASSERT_EQ(sc_agent_from_config(&agent, &a, prov, NULL, 0, NULL, NULL, NULL, NULL,
                                      "gpt-4o-mini", 10, "openai", 6, 0.7, ".", 1, 5, 20, false,
                                      2, NULL, 0, NULL),
                 SC_OK);
    sc_agent_set_mailbox(&agent, mb);

    uint64_t agent_id = (uint64_t)(uintptr_t)&agent;
    SC_ASSERT_EQ(sc_mailbox_send(mb, 999, agent_id, SC_MSG_TASK, "API ready at /api/checkout",
                                 24, 0),
                 SC_OK);

    sc_message_t msg = {0};
    SC_ASSERT_EQ(sc_mailbox_recv(mb, agent_id, &msg), SC_OK);
    SC_ASSERT_EQ(msg.type, SC_MSG_TASK);
    SC_ASSERT_EQ(msg.from_agent, 999u);
    SC_ASSERT_NOT_NULL(msg.payload);
    SC_ASSERT_EQ(msg.payload_len, 24u);
    sc_message_free(&a, &msg);

    sc_agent_deinit(&agent);
    sc_mailbox_destroy(mb);
}

static void test_mailbox_register_send_recv_with_agent_id(void) {
    sc_allocator_t a = sc_system_allocator();
    sc_mailbox_t *mb = sc_mailbox_create(&a, 8);
    SC_ASSERT_NOT_NULL(mb);

    uint64_t agent_id = 0x1234;
    SC_ASSERT_EQ(sc_mailbox_register(mb, agent_id), SC_OK);
    SC_ASSERT_EQ(sc_mailbox_send(mb, 999, agent_id, SC_MSG_TASK, "API ready", 9, 0), SC_OK);

    sc_message_t msg = {0};
    SC_ASSERT_EQ(sc_mailbox_recv(mb, agent_id, &msg), SC_OK);
    SC_ASSERT_EQ(msg.type, SC_MSG_TASK);
    SC_ASSERT_EQ(msg.from_agent, 999u);
    SC_ASSERT_NOT_NULL(msg.payload);
    SC_ASSERT_EQ(msg.payload_len, 9u);
    sc_message_free(&a, &msg);

    sc_mailbox_unregister(mb, agent_id);
    sc_mailbox_destroy(mb);
}

static void test_cancel_message_sets_cancel_requested(void) {
    sc_allocator_t a = sc_system_allocator();
    sc_mailbox_t *mb = sc_mailbox_create(&a, 8);
    SC_ASSERT_NOT_NULL(mb);

    sc_provider_t prov = {0};
    SC_ASSERT_EQ(sc_provider_create(&a, "openai", 6, "test-key", 8, "", 0, &prov), SC_OK);

    sc_agent_t agent = {0};
    SC_ASSERT_EQ(sc_agent_from_config(&agent, &a, prov, NULL, 0, NULL, NULL, NULL, NULL,
                                      "gpt-4o-mini", 10, "openai", 6, 0.7, ".", 1, 5, 20, false,
                                      2, NULL, 0, NULL),
                 SC_OK);
    sc_agent_set_mailbox(&agent, mb);
    SC_ASSERT_EQ(agent.cancel_requested, 0);

    uint64_t agent_id = (uint64_t)(uintptr_t)&agent;
    SC_ASSERT_EQ(sc_mailbox_send(mb, 1, agent_id, SC_MSG_CANCEL, "stop", 4, 0), SC_OK);

    sc_message_t msg = {0};
    SC_ASSERT_EQ(sc_mailbox_recv(mb, agent_id, &msg), SC_OK);
    SC_ASSERT_EQ(msg.type, SC_MSG_CANCEL);
    agent.cancel_requested = 1;
    sc_message_free(&a, &msg);

    SC_ASSERT_EQ(agent.cancel_requested, 1);

    sc_agent_deinit(&agent);
    sc_mailbox_destroy(mb);
}

/* ── Task list ────────────────────────────────────────────────────────── */
static void test_task_list_add_claim_complete_flow(void) {
    sc_allocator_t a = sc_system_allocator();
    sc_task_list_t *list = sc_task_list_create(&a, 16);
    SC_ASSERT_NOT_NULL(list);

    uint64_t id = 0;
    SC_ASSERT_EQ(sc_task_list_add(list, "Deploy API", "Deploy the checkout API", NULL, 0, &id),
                 SC_OK);
    SC_ASSERT_EQ(id, 1u);

    SC_ASSERT_EQ(sc_task_list_claim(list, 1, 100), SC_OK);
    sc_task_t t = {0};
    SC_ASSERT_EQ(sc_task_list_get(list, 1, &t), SC_OK);
    SC_ASSERT_EQ(t.status, SC_TASK_LIST_CLAIMED);
    SC_ASSERT_EQ(t.owner_agent_id, 100u);
    sc_task_free(&a, &t);

    SC_ASSERT_EQ(sc_task_list_update_status(list, 1, SC_TASK_LIST_COMPLETED), SC_OK);
    SC_ASSERT_EQ(sc_task_list_count_by_status(list, SC_TASK_LIST_COMPLETED), 1u);

    sc_task_list_destroy(list);
}

static void test_task_blocked_by_incomplete_dependency_stays_blocked(void) {
    sc_allocator_t a = sc_system_allocator();
    sc_task_list_t *list = sc_task_list_create(&a, 16);
    SC_ASSERT_NOT_NULL(list);

    uint64_t id1 = 0, id2 = 0;
    SC_ASSERT_EQ(sc_task_list_add(list, "Task A", "First", NULL, 0, &id1), SC_OK);
    SC_ASSERT_EQ(id1, 1u);

    uint64_t deps[] = {1};
    SC_ASSERT_EQ(sc_task_list_add(list, "Task B", "Depends on A", deps, 1, &id2), SC_OK);
    SC_ASSERT_EQ(id2, 2u);

    SC_ASSERT_TRUE(sc_task_list_is_blocked(list, 2));

    sc_task_t next = {0};
    SC_ASSERT_EQ(sc_task_list_next_available(list, &next), SC_OK);
    SC_ASSERT_EQ(next.id, 1u);
    sc_task_free(&a, &next);

    sc_task_list_destroy(list);
}

static void test_task_unblocks_when_dependency_completes(void) {
    sc_allocator_t a = sc_system_allocator();
    sc_task_list_t *list = sc_task_list_create(&a, 16);
    SC_ASSERT_NOT_NULL(list);

    uint64_t id1 = 0, id2 = 0;
    SC_ASSERT_EQ(sc_task_list_add(list, "Task A", "First", NULL, 0, &id1), SC_OK);
    uint64_t deps[] = {1};
    SC_ASSERT_EQ(sc_task_list_add(list, "Task B", "Depends on A", deps, 1, &id2), SC_OK);

    SC_ASSERT_TRUE(sc_task_list_is_blocked(list, 2));
    SC_ASSERT_EQ(sc_task_list_update_status(list, 1, SC_TASK_LIST_COMPLETED), SC_OK);
    SC_ASSERT_FALSE(sc_task_list_is_blocked(list, 2));

    sc_task_t next = {0};
    SC_ASSERT_EQ(sc_task_list_next_available(list, &next), SC_OK);
    SC_ASSERT_EQ(next.id, 2u);
    sc_task_free(&a, &next);

    sc_task_list_destroy(list);
}

static void test_next_available_skips_blocked_tasks(void) {
    sc_allocator_t a = sc_system_allocator();
    sc_task_list_t *list = sc_task_list_create(&a, 16);
    SC_ASSERT_NOT_NULL(list);

    uint64_t id1 = 0, id2 = 0;
    SC_ASSERT_EQ(sc_task_list_add(list, "Unblocked", "No deps", NULL, 0, &id1), SC_OK);
    uint64_t deps[] = {1};
    SC_ASSERT_EQ(sc_task_list_add(list, "Blocked", "Depends on 1", deps, 1, &id2), SC_OK);

    sc_task_t next = {0};
    SC_ASSERT_EQ(sc_task_list_next_available(list, &next), SC_OK);
    SC_ASSERT_EQ(next.id, 1u);
    sc_task_free(&a, &next);

    sc_task_list_destroy(list);
}

static void test_claim_fails_on_already_claimed_task(void) {
    sc_allocator_t a = sc_system_allocator();
    sc_task_list_t *list = sc_task_list_create(&a, 16);
    SC_ASSERT_NOT_NULL(list);

    uint64_t id = 0;
    SC_ASSERT_EQ(sc_task_list_add(list, "Task", "Desc", NULL, 0, &id), SC_OK);
    SC_ASSERT_EQ(sc_task_list_claim(list, 1, 100), SC_OK);
    SC_ASSERT_EQ(sc_task_list_claim(list, 1, 200), SC_ERR_ALREADY_EXISTS);

    sc_task_list_destroy(list);
}

void run_agent_teams_tests(void) {
    SC_TEST_SUITE("agent teams (worktree + team config + mailbox + task list)");
    SC_RUN_TEST(test_mailbox_register_send_recv_with_agent_id);
    SC_RUN_TEST(test_mailbox_recv_in_agent_context_works);
    SC_RUN_TEST(test_cancel_message_sets_cancel_requested);
    SC_RUN_TEST(test_task_list_add_claim_complete_flow);
    SC_RUN_TEST(test_task_blocked_by_incomplete_dependency_stays_blocked);
    SC_RUN_TEST(test_task_unblocks_when_dependency_completes);
    SC_RUN_TEST(test_next_available_skips_blocked_tasks);
    SC_RUN_TEST(test_claim_fails_on_already_claimed_task);
    SC_RUN_TEST(test_worktree_create_generates_correct_path_and_branch);
    SC_RUN_TEST(test_worktree_remove_marks_inactive);
    SC_RUN_TEST(test_worktree_list_returns_active_only);
    SC_RUN_TEST(test_worktree_get_finds_by_agent_id);
    SC_RUN_TEST(test_team_config_parse_with_3_members);
    SC_RUN_TEST(test_team_config_get_member_by_name);
    SC_RUN_TEST(test_team_config_get_by_role_finds_first_match);
    SC_RUN_TEST(test_team_config_parse_autonomy_levels_maps_correctly);
    SC_RUN_TEST(test_team_config_parse_missing_fields_uses_defaults);
}
