#include "human/agent/hula.h"
#include "human/agent/idempotency.h"
#include "human/core/allocator.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include "human/tool.h"
#include "test_framework.h"
#include <string.h>

/* Mock tool for testing: echo tool */
static hu_error_t mock_echo_execute(void *ctx, hu_allocator_t *alloc, const hu_json_value_t *args,
                                    hu_tool_result_t *out) {
    (void)ctx;
    /* Simple echo: return the input as-is */
    char *result = NULL;
    size_t result_len = 0;
    hu_error_t err = hu_json_stringify(alloc, args, &result, &result_len);
    if (err != HU_OK)
        return err;

    *out = hu_tool_result_ok_owned(result, result_len);
    return HU_OK;
}

static const char *mock_echo_name(void *ctx) {
    (void)ctx;
    return "echo";
}

static const char *mock_echo_description(void *ctx) {
    (void)ctx;
    return "Echo tool for testing";
}

static const char *mock_echo_parameters(void *ctx) {
    (void)ctx;
    return "{}";
}

static const hu_tool_vtable_t mock_echo_vtable = {
    .execute = mock_echo_execute,
    .name = mock_echo_name,
    .description = mock_echo_description,
    .parameters_json = mock_echo_parameters,
    .deinit = NULL,
    .execute_streaming = NULL,
};

/* Test: HuLa CALL with idempotency dedup */
static void test_hula_call_with_idempotency_dedup(void) {
    hu_allocator_t alloc = hu_system_allocator();

    hu_tool_t tool = {.ctx = NULL, .vtable = &mock_echo_vtable};

    hu_hula_program_t prog;
    hu_error_t err = hu_hula_program_init(&prog, alloc, "test_program", 12);
    HU_ASSERT_EQ(err, HU_OK);

    hu_hula_node_t *call_node = hu_hula_program_alloc_node(&prog, HU_HULA_CALL, "call1");
    HU_ASSERT_NOT_NULL(call_node);
    call_node->tool_name = hu_strdup(&alloc, "echo");
    call_node->args_json = hu_strdup(&alloc, "{\"message\": \"hello\"}");
    HU_ASSERT_NOT_NULL(call_node->tool_name);
    HU_ASSERT_NOT_NULL(call_node->args_json);

    prog.root = call_node;

    hu_hula_exec_t exec;
    err = hu_hula_exec_init(&exec, alloc, &prog, &tool, 1);
    HU_ASSERT_EQ(err, HU_OK);

    hu_idempotency_registry_t *reg = NULL;
    err = hu_idempotency_create(&alloc, &reg);
    HU_ASSERT_EQ(err, HU_OK);
    hu_hula_exec_set_idempotency_registry(&exec, reg);

    /* First execution — should call tool */
    err = hu_hula_exec_run(&exec);
    HU_ASSERT_EQ(err, HU_OK);

    const hu_hula_result_t *result1 = hu_hula_exec_result(&exec, "call1");
    HU_ASSERT_NOT_NULL(result1);
    HU_ASSERT_EQ(result1->status, HU_HULA_DONE);
    HU_ASSERT_STR_CONTAINS(result1->output, "hello");

    /* Save output before deinit (exec_deinit frees result pointers) */
    char saved_output[256];
    size_t olen = strlen(result1->output);
    if (olen >= sizeof(saved_output))
        olen = sizeof(saved_output) - 1;
    memcpy(saved_output, result1->output, olen);
    saved_output[olen] = '\0';

    hu_idempotency_stats_t stats1;
    hu_idempotency_stats(reg, &stats1);
    HU_ASSERT_EQ(stats1.entry_count, 1);
    HU_ASSERT_EQ(stats1.total_hits, 0);
    HU_ASSERT_EQ(stats1.total_misses, 1);

    hu_hula_exec_deinit(&exec);

    /* Second execution with same args — should use cache */
    err = hu_hula_exec_init(&exec, alloc, &prog, &tool, 1);
    HU_ASSERT_EQ(err, HU_OK);
    hu_hula_exec_set_idempotency_registry(&exec, reg);

    err = hu_hula_exec_run(&exec);
    HU_ASSERT_EQ(err, HU_OK);

    const hu_hula_result_t *result2 = hu_hula_exec_result(&exec, "call1");
    HU_ASSERT_NOT_NULL(result2);
    HU_ASSERT_EQ(result2->status, HU_HULA_DONE);
    HU_ASSERT_STR_EQ(result2->output, saved_output);

    hu_idempotency_stats(reg, &stats1);
    HU_ASSERT_EQ(stats1.total_hits, 1);
    HU_ASSERT_EQ(stats1.total_misses, 1); /* cumulative: 1 miss from first run */

    hu_hula_exec_deinit(&exec);
    hu_idempotency_destroy(reg, &alloc);
    hu_hula_program_deinit(&prog);
}

/* Test: HuLa SEQ with mixed cache hits/misses */
static void test_hula_seq_with_idempotency(void) {
    hu_allocator_t alloc = hu_system_allocator();

    hu_tool_t tool = {.ctx = NULL, .vtable = &mock_echo_vtable};

    hu_hula_program_t prog;
    hu_error_t err = hu_hula_program_init(&prog, alloc, "test_seq", 8);
    HU_ASSERT_EQ(err, HU_OK);

    hu_hula_node_t *seq = hu_hula_program_alloc_node(&prog, HU_HULA_SEQ, "seq1");
    HU_ASSERT_NOT_NULL(seq);

    hu_hula_node_t *call1 = hu_hula_program_alloc_node(&prog, HU_HULA_CALL, "call1");
    HU_ASSERT_NOT_NULL(call1);
    call1->tool_name = hu_strdup(&alloc, "echo");
    call1->args_json = hu_strdup(&alloc, "{\"msg\": \"one\"}");
    seq->children[seq->children_count++] = call1;

    hu_hula_node_t *call2 = hu_hula_program_alloc_node(&prog, HU_HULA_CALL, "call2");
    HU_ASSERT_NOT_NULL(call2);
    call2->tool_name = hu_strdup(&alloc, "echo");
    call2->args_json = hu_strdup(&alloc, "{\"msg\": \"two\"}");
    seq->children[seq->children_count++] = call2;

    prog.root = seq;

    hu_hula_exec_t exec;
    err = hu_hula_exec_init(&exec, alloc, &prog, &tool, 1);
    HU_ASSERT_EQ(err, HU_OK);

    hu_idempotency_registry_t *reg = NULL;
    err = hu_idempotency_create(&alloc, &reg);
    HU_ASSERT_EQ(err, HU_OK);
    hu_hula_exec_set_idempotency_registry(&exec, reg);

    /* First run */
    err = hu_hula_exec_run(&exec);
    HU_ASSERT_EQ(err, HU_OK);

    const hu_hula_result_t *r1_1 = hu_hula_exec_result(&exec, "call1");
    const hu_hula_result_t *r1_2 = hu_hula_exec_result(&exec, "call2");
    HU_ASSERT_NOT_NULL(r1_1);
    HU_ASSERT_NOT_NULL(r1_2);

    /* Save outputs before deinit (exec_deinit frees result pointers) */
    char saved_out1[256], saved_out2[256];
    size_t o1 = strlen(r1_1->output);
    size_t o2 = strlen(r1_2->output);
    if (o1 >= sizeof(saved_out1))
        o1 = sizeof(saved_out1) - 1;
    if (o2 >= sizeof(saved_out2))
        o2 = sizeof(saved_out2) - 1;
    memcpy(saved_out1, r1_1->output, o1);
    saved_out1[o1] = '\0';
    memcpy(saved_out2, r1_2->output, o2);
    saved_out2[o2] = '\0';

    hu_idempotency_stats_t stats;
    hu_idempotency_stats(reg, &stats);
    HU_ASSERT_EQ(stats.entry_count, 2);
    HU_ASSERT_EQ(stats.total_misses, 2);
    HU_ASSERT_EQ(stats.total_hits, 0);

    hu_hula_exec_deinit(&exec);

    /* Second run: both should hit cache */
    err = hu_hula_exec_init(&exec, alloc, &prog, &tool, 1);
    HU_ASSERT_EQ(err, HU_OK);
    hu_hula_exec_set_idempotency_registry(&exec, reg);

    err = hu_hula_exec_run(&exec);
    HU_ASSERT_EQ(err, HU_OK);

    const hu_hula_result_t *r2_1 = hu_hula_exec_result(&exec, "call1");
    const hu_hula_result_t *r2_2 = hu_hula_exec_result(&exec, "call2");
    HU_ASSERT_STR_EQ(r2_1->output, saved_out1);
    HU_ASSERT_STR_EQ(r2_2->output, saved_out2);

    hu_idempotency_stats(reg, &stats);
    HU_ASSERT_EQ(stats.total_hits, 2);

    hu_hula_exec_deinit(&exec);
    hu_idempotency_destroy(reg, &alloc);
    hu_hula_program_deinit(&prog);
}

/* Test: Idempotency clear resets cache for new workflow */
static void test_idempotency_clear_for_new_workflow(void) {
    hu_allocator_t alloc = hu_system_allocator();

    hu_tool_t tool = {.ctx = NULL, .vtable = &mock_echo_vtable};

    hu_hula_program_t prog;
    hu_error_t err = hu_hula_program_init(&prog, alloc, "test_clear", 10);
    HU_ASSERT_EQ(err, HU_OK);

    hu_hula_node_t *call_node = hu_hula_program_alloc_node(&prog, HU_HULA_CALL, "call1");
    HU_ASSERT_NOT_NULL(call_node);
    call_node->tool_name = hu_strdup(&alloc, "echo");
    call_node->args_json = hu_strdup(&alloc, "{\"data\": \"test\"}");
    prog.root = call_node;

    hu_hula_exec_t exec;
    err = hu_hula_exec_init(&exec, alloc, &prog, &tool, 1);
    HU_ASSERT_EQ(err, HU_OK);

    hu_idempotency_registry_t *reg = NULL;
    err = hu_idempotency_create(&alloc, &reg);
    HU_ASSERT_EQ(err, HU_OK);
    hu_hula_exec_set_idempotency_registry(&exec, reg);

    /* Run 1: populate cache */
    err = hu_hula_exec_run(&exec);
    HU_ASSERT_EQ(err, HU_OK);

    hu_idempotency_stats_t stats;
    hu_idempotency_stats(reg, &stats);
    HU_ASSERT_EQ(stats.entry_count, 1);

    /* Clear for new workflow */
    hu_idempotency_clear(reg, &alloc);

    hu_idempotency_stats(reg, &stats);
    HU_ASSERT_EQ(stats.entry_count, 0);
    HU_ASSERT_EQ(stats.total_hits, 0);
    HU_ASSERT_EQ(stats.total_misses, 0);

    hu_hula_exec_deinit(&exec);
    hu_idempotency_destroy(reg, &alloc);
    hu_hula_program_deinit(&prog);
}

void run_idempotency_hula_integration_tests(void) {
    HU_TEST_SUITE("idempotency_hula_integration");
    HU_RUN_TEST(test_hula_call_with_idempotency_dedup);
    HU_RUN_TEST(test_hula_seq_with_idempotency);
    HU_RUN_TEST(test_idempotency_clear_for_new_workflow);
}
