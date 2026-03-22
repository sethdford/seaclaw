#include "human/agent/hula.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include "test_framework.h"
#include <string.h>

/* ── Stub tool for testing ──────────────────────────────────────────────── */

static hu_error_t echo_execute(void *ctx, hu_allocator_t *alloc,
                                const hu_json_value_t *args, hu_tool_result_t *out) {
    (void)ctx;
    const char *text = hu_json_get_string(args, "text");
    if (text) {
        size_t len = strlen(text);
        char *dup = hu_strndup(alloc, text, len);
        *out = hu_tool_result_ok_owned(dup, len);
    } else {
        *out = hu_tool_result_ok("echo", 4);
    }
    return HU_OK;
}

static const char *echo_name(void *ctx) { (void)ctx; return "echo"; }
static const char *echo_desc(void *ctx) { (void)ctx; return "Echoes text"; }
static const char *echo_params(void *ctx) { (void)ctx; return "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"}}}"; }

static const hu_tool_vtable_t echo_vtable = {
    .execute = echo_execute, .name = echo_name,
    .description = echo_desc, .parameters_json = echo_params,
};

static hu_error_t fail_execute(void *ctx, hu_allocator_t *alloc,
                                const hu_json_value_t *args, hu_tool_result_t *out) {
    (void)ctx; (void)alloc; (void)args;
    *out = hu_tool_result_fail("intentional failure", 19);
    return HU_OK;
}

static const char *fail_name(void *ctx) { (void)ctx; return "fail_tool"; }
static const char *fail_desc(void *ctx) { (void)ctx; return "Always fails"; }
static const char *fail_params(void *ctx) { (void)ctx; return "{}"; }

static const hu_tool_vtable_t fail_vtable = {
    .execute = fail_execute, .name = fail_name,
    .description = fail_desc, .parameters_json = fail_params,
};

static hu_tool_t make_tools(hu_tool_t *buf) {
    (void)buf;
    buf[0] = (hu_tool_t){.ctx = NULL, .vtable = &echo_vtable};
    buf[1] = (hu_tool_t){.ctx = NULL, .vtable = &fail_vtable};
    return buf[0];
}

/* ── Op name tests ──────────────────────────────────────────────────────── */

static void hula_op_names_correct(void) {
    HU_ASSERT_STR_EQ(hu_hula_op_name(HU_HULA_CALL), "call");
    HU_ASSERT_STR_EQ(hu_hula_op_name(HU_HULA_SEQ), "seq");
    HU_ASSERT_STR_EQ(hu_hula_op_name(HU_HULA_PAR), "par");
    HU_ASSERT_STR_EQ(hu_hula_op_name(HU_HULA_BRANCH), "branch");
    HU_ASSERT_STR_EQ(hu_hula_op_name(HU_HULA_LOOP), "loop");
    HU_ASSERT_STR_EQ(hu_hula_op_name(HU_HULA_DELEGATE), "delegate");
    HU_ASSERT_STR_EQ(hu_hula_op_name(HU_HULA_EMIT), "emit");
}

static void hula_pred_names_correct(void) {
    HU_ASSERT_STR_EQ(hu_hula_pred_name(HU_HULA_PRED_SUCCESS), "success");
    HU_ASSERT_STR_EQ(hu_hula_pred_name(HU_HULA_PRED_FAILURE), "failure");
    HU_ASSERT_STR_EQ(hu_hula_pred_name(HU_HULA_PRED_CONTAINS), "contains");
    HU_ASSERT_STR_EQ(hu_hula_pred_name(HU_HULA_PRED_ALWAYS), "always");
}

static void hula_status_names_correct(void) {
    HU_ASSERT_STR_EQ(hu_hula_status_name(HU_HULA_PENDING), "pending");
    HU_ASSERT_STR_EQ(hu_hula_status_name(HU_HULA_DONE), "done");
    HU_ASSERT_STR_EQ(hu_hula_status_name(HU_HULA_FAILED), "failed");
    HU_ASSERT_STR_EQ(hu_hula_status_name(HU_HULA_SKIPPED), "skipped");
}

/* ── Program lifecycle ──────────────────────────────────────────────────── */

static void hula_program_init_sets_name(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_hula_program_t prog;
    hu_error_t err = hu_hula_program_init(&prog, alloc, "test_prog", 9);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(prog.name, "test_prog");
    HU_ASSERT_EQ(prog.version, HU_HULA_VERSION);
    hu_hula_program_deinit(&prog);
}

static void hula_program_alloc_node_returns_node(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_hula_program_t prog;
    hu_hula_program_init(&prog, alloc, "test", 4);

    hu_hula_node_t *n = hu_hula_program_alloc_node(&prog, HU_HULA_CALL, "n1");
    HU_ASSERT_NOT_NULL(n);
    HU_ASSERT_EQ(n->op, HU_HULA_CALL);
    HU_ASSERT_STR_EQ(n->id, "n1");
    HU_ASSERT_EQ(prog.node_count, 1u);

    hu_hula_program_deinit(&prog);
}

static void hula_program_alloc_node_respects_max(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_hula_program_t prog;
    hu_hula_program_init(&prog, alloc, "test", 4);

    for (size_t i = 0; i < HU_HULA_MAX_NODES; i++) {
        char id[16];
        (void)snprintf(id, sizeof(id), "n%zu", i);
        HU_ASSERT_NOT_NULL(hu_hula_program_alloc_node(&prog, HU_HULA_CALL, id));
    }
    HU_ASSERT_NULL(hu_hula_program_alloc_node(&prog, HU_HULA_CALL, "overflow"));

    hu_hula_program_deinit(&prog);
}

/* ── Parse JSON ─────────────────────────────────────────────────────────── */

static void hula_parse_simple_call(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *json =
        "{\"name\":\"greet\",\"version\":1,"
        "\"root\":{\"op\":\"call\",\"id\":\"c1\",\"tool\":\"echo\","
        "\"args\":{\"text\":\"hello\"}}}";
    hu_hula_program_t prog;
    hu_error_t err = hu_hula_parse_json(&alloc, json, strlen(json), &prog);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(prog.name, "greet");
    HU_ASSERT_NOT_NULL(prog.root);
    HU_ASSERT_EQ(prog.root->op, HU_HULA_CALL);
    HU_ASSERT_STR_EQ(prog.root->id, "c1");
    HU_ASSERT_STR_EQ(prog.root->tool_name, "echo");
    HU_ASSERT_NOT_NULL(prog.root->args_json);

    hu_hula_program_deinit(&prog);
}

static void hula_parse_seq_with_children(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *json =
        "{\"name\":\"pipeline\",\"root\":{\"op\":\"seq\",\"id\":\"s1\","
        "\"children\":["
        "{\"op\":\"call\",\"id\":\"c1\",\"tool\":\"echo\",\"args\":{\"text\":\"a\"}},"
        "{\"op\":\"call\",\"id\":\"c2\",\"tool\":\"echo\",\"args\":{\"text\":\"b\"}}"
        "]}}";
    hu_hula_program_t prog;
    hu_error_t err = hu_hula_parse_json(&alloc, json, strlen(json), &prog);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(prog.root->op, HU_HULA_SEQ);
    HU_ASSERT_EQ(prog.root->children_count, 2u);
    HU_ASSERT_STR_EQ(prog.root->children[0]->id, "c1");
    HU_ASSERT_STR_EQ(prog.root->children[1]->id, "c2");

    hu_hula_program_deinit(&prog);
}

static void hula_parse_branch(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *json =
        "{\"name\":\"cond\",\"root\":{\"op\":\"branch\",\"id\":\"b1\","
        "\"pred\":\"success\","
        "\"then\":{\"op\":\"call\",\"id\":\"t1\",\"tool\":\"echo\",\"args\":{}},"
        "\"else\":{\"op\":\"call\",\"id\":\"e1\",\"tool\":\"echo\",\"args\":{}}"
        "}}";
    hu_hula_program_t prog;
    hu_error_t err = hu_hula_parse_json(&alloc, json, strlen(json), &prog);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(prog.root->op, HU_HULA_BRANCH);
    HU_ASSERT_EQ(prog.root->pred, HU_HULA_PRED_SUCCESS);
    HU_ASSERT_EQ(prog.root->children_count, 2u);

    hu_hula_program_deinit(&prog);
}

static void hula_parse_loop(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *json =
        "{\"name\":\"retry\",\"root\":{\"op\":\"loop\",\"id\":\"l1\","
        "\"pred\":\"always\",\"max_iter\":3,"
        "\"body\":{\"op\":\"call\",\"id\":\"c1\",\"tool\":\"echo\",\"args\":{\"text\":\"x\"}}"
        "}}";
    hu_hula_program_t prog;
    hu_error_t err = hu_hula_parse_json(&alloc, json, strlen(json), &prog);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(prog.root->op, HU_HULA_LOOP);
    HU_ASSERT_EQ(prog.root->max_iter, 3u);
    HU_ASSERT_EQ(prog.root->pred, HU_HULA_PRED_ALWAYS);
    HU_ASSERT_EQ(prog.root->children_count, 1u);

    hu_hula_program_deinit(&prog);
}

static void hula_parse_delegate(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *json =
        "{\"name\":\"spawn\",\"root\":{\"op\":\"delegate\",\"id\":\"d1\","
        "\"goal\":\"search the web\",\"model\":\"gpt-4\"}}";
    hu_hula_program_t prog;
    hu_error_t err = hu_hula_parse_json(&alloc, json, strlen(json), &prog);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(prog.root->op, HU_HULA_DELEGATE);
    HU_ASSERT_STR_EQ(prog.root->goal, "search the web");
    HU_ASSERT_STR_EQ(prog.root->delegate_model, "gpt-4");

    hu_hula_program_deinit(&prog);
}

static void hula_parse_emit(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *json =
        "{\"name\":\"out\",\"root\":{\"op\":\"emit\",\"id\":\"e1\","
        "\"emit_key\":\"response\",\"emit_value\":\"done\"}}";
    hu_hula_program_t prog;
    hu_error_t err = hu_hula_parse_json(&alloc, json, strlen(json), &prog);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(prog.root->op, HU_HULA_EMIT);
    HU_ASSERT_STR_EQ(prog.root->emit_key, "response");
    HU_ASSERT_STR_EQ(prog.root->emit_value, "done");

    hu_hula_program_deinit(&prog);
}

static void hula_parse_invalid_json_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_hula_program_t prog;
    hu_error_t err = hu_hula_parse_json(&alloc, "not json", 8, &prog);
    HU_ASSERT_NEQ(err, HU_OK);
}

static void hula_parse_missing_root_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_hula_program_t prog;
    const char *json = "{\"name\":\"empty\"}";
    hu_error_t err = hu_hula_parse_json(&alloc, json, strlen(json), &prog);
    HU_ASSERT_NEQ(err, HU_OK);
}

/* ── Validate ───────────────────────────────────────────────────────────── */

static void hula_validate_valid_call(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_hula_program_t prog;
    hu_hula_program_init(&prog, alloc, "v", 1);
    hu_hula_node_t *n = hu_hula_program_alloc_node(&prog, HU_HULA_CALL, "c1");
    n->tool_name = hu_strdup(&alloc, "echo");
    prog.root = n;

    const char *tools[] = {"echo", "fail_tool"};
    hu_hula_validation_t v;
    hu_hula_validate(&prog, &alloc, tools, 2, &v);
    HU_ASSERT_TRUE(v.valid);
    HU_ASSERT_EQ(v.diag_count, 0u);

    hu_hula_validation_deinit(&alloc, &v);
    hu_hula_program_deinit(&prog);
}

static void hula_validate_unknown_tool(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_hula_program_t prog;
    hu_hula_program_init(&prog, alloc, "v", 1);
    hu_hula_node_t *n = hu_hula_program_alloc_node(&prog, HU_HULA_CALL, "c1");
    n->tool_name = hu_strdup(&alloc, "nonexistent");
    prog.root = n;

    const char *tools[] = {"echo"};
    hu_hula_validation_t v;
    hu_hula_validate(&prog, &alloc, tools, 1, &v);
    HU_ASSERT_FALSE(v.valid);
    HU_ASSERT_GT(v.diag_count, 0u);

    hu_hula_validation_deinit(&alloc, &v);
    hu_hula_program_deinit(&prog);
}

static void hula_validate_call_missing_tool(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_hula_program_t prog;
    hu_hula_program_init(&prog, alloc, "v", 1);
    hu_hula_node_t *n = hu_hula_program_alloc_node(&prog, HU_HULA_CALL, "c1");
    prog.root = n;

    hu_hula_validation_t v;
    hu_hula_validate(&prog, &alloc, NULL, 0, &v);
    HU_ASSERT_FALSE(v.valid);

    hu_hula_validation_deinit(&alloc, &v);
    hu_hula_program_deinit(&prog);
}

static void hula_validate_seq_no_children(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_hula_program_t prog;
    hu_hula_program_init(&prog, alloc, "v", 1);
    hu_hula_node_t *n = hu_hula_program_alloc_node(&prog, HU_HULA_SEQ, "s1");
    prog.root = n;

    hu_hula_validation_t v;
    hu_hula_validate(&prog, &alloc, NULL, 0, &v);
    HU_ASSERT_FALSE(v.valid);

    hu_hula_validation_deinit(&alloc, &v);
    hu_hula_program_deinit(&prog);
}

static void hula_validate_no_root(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_hula_program_t prog;
    hu_hula_program_init(&prog, alloc, "v", 1);

    hu_hula_validation_t v;
    hu_hula_validate(&prog, &alloc, NULL, 0, &v);
    HU_ASSERT_FALSE(v.valid);

    hu_hula_validation_deinit(&alloc, &v);
    hu_hula_program_deinit(&prog);
}

static void hula_validate_delegate_missing_goal(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_hula_program_t prog;
    hu_hula_program_init(&prog, alloc, "v", 1);
    hu_hula_node_t *n = hu_hula_program_alloc_node(&prog, HU_HULA_DELEGATE, "d1");
    prog.root = n;

    hu_hula_validation_t v;
    hu_hula_validate(&prog, &alloc, NULL, 0, &v);
    HU_ASSERT_FALSE(v.valid);

    hu_hula_validation_deinit(&alloc, &v);
    hu_hula_program_deinit(&prog);
}

static void hula_validate_emit_missing_key(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_hula_program_t prog;
    hu_hula_program_init(&prog, alloc, "v", 1);
    hu_hula_node_t *n = hu_hula_program_alloc_node(&prog, HU_HULA_EMIT, "e1");
    prog.root = n;

    hu_hula_validation_t v;
    hu_hula_validate(&prog, &alloc, NULL, 0, &v);
    HU_ASSERT_FALSE(v.valid);

    hu_hula_validation_deinit(&alloc, &v);
    hu_hula_program_deinit(&prog);
}

/* ── Execute ────────────────────────────────────────────────────────────── */

static void hula_exec_simple_call(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *json =
        "{\"name\":\"run\",\"root\":{\"op\":\"call\",\"id\":\"c1\","
        "\"tool\":\"echo\",\"args\":{\"text\":\"hi\"}}}";
    hu_hula_program_t prog;
    HU_ASSERT_EQ(hu_hula_parse_json(&alloc, json, strlen(json), &prog), HU_OK);

    hu_tool_t tools[2];
    make_tools(tools);

    hu_hula_exec_t exec;
    HU_ASSERT_EQ(hu_hula_exec_init(&exec, alloc, &prog, tools, 2), HU_OK);
    HU_ASSERT_EQ(hu_hula_exec_run(&exec), HU_OK);

    const hu_hula_result_t *r = hu_hula_exec_result(&exec, "c1");
    HU_ASSERT_NOT_NULL(r);
    HU_ASSERT_EQ(r->status, HU_HULA_DONE);
    HU_ASSERT_STR_EQ(r->output, "hi");

    hu_hula_exec_deinit(&exec);
    hu_hula_program_deinit(&prog);
}

static void hula_exec_seq_propagates_output(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *json =
        "{\"name\":\"pipe\",\"root\":{\"op\":\"seq\",\"id\":\"s1\","
        "\"children\":["
        "{\"op\":\"call\",\"id\":\"c1\",\"tool\":\"echo\",\"args\":{\"text\":\"first\"}},"
        "{\"op\":\"call\",\"id\":\"c2\",\"tool\":\"echo\",\"args\":{\"text\":\"second\"}}"
        "]}}";
    hu_hula_program_t prog;
    HU_ASSERT_EQ(hu_hula_parse_json(&alloc, json, strlen(json), &prog), HU_OK);

    hu_tool_t tools[2];
    make_tools(tools);
    hu_hula_exec_t exec;
    HU_ASSERT_EQ(hu_hula_exec_init(&exec, alloc, &prog, tools, 2), HU_OK);
    HU_ASSERT_EQ(hu_hula_exec_run(&exec), HU_OK);

    const hu_hula_result_t *rs = hu_hula_exec_result(&exec, "s1");
    HU_ASSERT_NOT_NULL(rs);
    HU_ASSERT_EQ(rs->status, HU_HULA_DONE);
    HU_ASSERT_STR_EQ(rs->output, "second");

    hu_hula_exec_deinit(&exec);
    hu_hula_program_deinit(&prog);
}

static void hula_exec_seq_short_circuits_on_failure(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *json =
        "{\"name\":\"fail_pipe\",\"root\":{\"op\":\"seq\",\"id\":\"s1\","
        "\"children\":["
        "{\"op\":\"call\",\"id\":\"c1\",\"tool\":\"fail_tool\",\"args\":{}},"
        "{\"op\":\"call\",\"id\":\"c2\",\"tool\":\"echo\",\"args\":{\"text\":\"unreachable\"}}"
        "]}}";
    hu_hula_program_t prog;
    HU_ASSERT_EQ(hu_hula_parse_json(&alloc, json, strlen(json), &prog), HU_OK);

    hu_tool_t tools[2];
    make_tools(tools);
    hu_hula_exec_t exec;
    HU_ASSERT_EQ(hu_hula_exec_init(&exec, alloc, &prog, tools, 2), HU_OK);
    HU_ASSERT_EQ(hu_hula_exec_run(&exec), HU_OK);

    const hu_hula_result_t *rs = hu_hula_exec_result(&exec, "s1");
    HU_ASSERT_NOT_NULL(rs);
    HU_ASSERT_EQ(rs->status, HU_HULA_FAILED);

    const hu_hula_result_t *r2 = hu_hula_exec_result(&exec, "c2");
    HU_ASSERT_NOT_NULL(r2);
    HU_ASSERT_EQ(r2->status, HU_HULA_PENDING);

    hu_hula_exec_deinit(&exec);
    hu_hula_program_deinit(&prog);
}

static void hula_exec_par_runs_all(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *json =
        "{\"name\":\"par\",\"root\":{\"op\":\"par\",\"id\":\"p1\","
        "\"children\":["
        "{\"op\":\"call\",\"id\":\"c1\",\"tool\":\"echo\",\"args\":{\"text\":\"a\"}},"
        "{\"op\":\"call\",\"id\":\"c2\",\"tool\":\"echo\",\"args\":{\"text\":\"b\"}}"
        "]}}";
    hu_hula_program_t prog;
    HU_ASSERT_EQ(hu_hula_parse_json(&alloc, json, strlen(json), &prog), HU_OK);

    hu_tool_t tools[2];
    make_tools(tools);
    hu_hula_exec_t exec;
    HU_ASSERT_EQ(hu_hula_exec_init(&exec, alloc, &prog, tools, 2), HU_OK);
    HU_ASSERT_EQ(hu_hula_exec_run(&exec), HU_OK);

    HU_ASSERT_EQ(hu_hula_exec_result(&exec, "c1")->status, HU_HULA_DONE);
    HU_ASSERT_EQ(hu_hula_exec_result(&exec, "c2")->status, HU_HULA_DONE);
    HU_ASSERT_EQ(hu_hula_exec_result(&exec, "p1")->status, HU_HULA_DONE);

    hu_hula_exec_deinit(&exec);
    hu_hula_program_deinit(&prog);
}

static void hula_exec_branch_takes_then_on_success(void) {
    hu_allocator_t alloc = hu_system_allocator();
    /* Seq: call echo (succeeds) → branch on success → then echo "yes" */
    const char *json =
        "{\"name\":\"br\",\"root\":{\"op\":\"seq\",\"id\":\"s1\","
        "\"children\":["
        "{\"op\":\"call\",\"id\":\"c1\",\"tool\":\"echo\",\"args\":{\"text\":\"ok\"}},"
        "{\"op\":\"branch\",\"id\":\"b1\",\"pred\":\"success\","
        "\"then\":{\"op\":\"call\",\"id\":\"t1\",\"tool\":\"echo\",\"args\":{\"text\":\"yes\"}},"
        "\"else\":{\"op\":\"call\",\"id\":\"e1\",\"tool\":\"echo\",\"args\":{\"text\":\"no\"}}"
        "}]}}";
    hu_hula_program_t prog;
    HU_ASSERT_EQ(hu_hula_parse_json(&alloc, json, strlen(json), &prog), HU_OK);

    hu_tool_t tools[2];
    make_tools(tools);
    hu_hula_exec_t exec;
    HU_ASSERT_EQ(hu_hula_exec_init(&exec, alloc, &prog, tools, 2), HU_OK);
    HU_ASSERT_EQ(hu_hula_exec_run(&exec), HU_OK);

    HU_ASSERT_EQ(hu_hula_exec_result(&exec, "t1")->status, HU_HULA_DONE);
    HU_ASSERT_STR_EQ(hu_hula_exec_result(&exec, "t1")->output, "yes");
    HU_ASSERT_EQ(hu_hula_exec_result(&exec, "e1")->status, HU_HULA_PENDING);

    hu_hula_exec_deinit(&exec);
    hu_hula_program_deinit(&prog);
}

static void hula_exec_branch_takes_else_on_failure(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *json =
        "{\"name\":\"br\",\"root\":{\"op\":\"seq\",\"id\":\"s1\","
        "\"children\":["
        "{\"op\":\"call\",\"id\":\"c1\",\"tool\":\"fail_tool\",\"args\":{}},"
        "{\"op\":\"branch\",\"id\":\"b1\",\"pred\":\"success\","
        "\"then\":{\"op\":\"call\",\"id\":\"t1\",\"tool\":\"echo\",\"args\":{\"text\":\"yes\"}},"
        "\"else\":{\"op\":\"call\",\"id\":\"e1\",\"tool\":\"echo\",\"args\":{\"text\":\"fallback\"}}"
        "}]}}";
    hu_hula_program_t prog;
    HU_ASSERT_EQ(hu_hula_parse_json(&alloc, json, strlen(json), &prog), HU_OK);

    hu_tool_t tools[2];
    make_tools(tools);
    hu_hula_exec_t exec;
    HU_ASSERT_EQ(hu_hula_exec_init(&exec, alloc, &prog, tools, 2), HU_OK);
    HU_ASSERT_EQ(hu_hula_exec_run(&exec), HU_OK);

    /* seq short-circuits on c1 failure, so branch doesn't run */
    HU_ASSERT_EQ(hu_hula_exec_result(&exec, "s1")->status, HU_HULA_FAILED);

    hu_hula_exec_deinit(&exec);
    hu_hula_program_deinit(&prog);
}

static void hula_exec_loop_bounded(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *json =
        "{\"name\":\"loop\",\"root\":{\"op\":\"loop\",\"id\":\"l1\","
        "\"pred\":\"always\",\"max_iter\":3,"
        "\"body\":{\"op\":\"call\",\"id\":\"c1\",\"tool\":\"echo\",\"args\":{\"text\":\"tick\"}}"
        "}}";
    hu_hula_program_t prog;
    HU_ASSERT_EQ(hu_hula_parse_json(&alloc, json, strlen(json), &prog), HU_OK);

    hu_tool_t tools[2];
    make_tools(tools);
    hu_hula_exec_t exec;
    HU_ASSERT_EQ(hu_hula_exec_init(&exec, alloc, &prog, tools, 2), HU_OK);
    HU_ASSERT_EQ(hu_hula_exec_run(&exec), HU_OK);

    HU_ASSERT_EQ(hu_hula_exec_result(&exec, "l1")->status, HU_HULA_DONE);

    hu_hula_exec_deinit(&exec);
    hu_hula_program_deinit(&prog);
}

static void hula_exec_delegate_stub(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *json =
        "{\"name\":\"spawn\",\"root\":{\"op\":\"delegate\",\"id\":\"d1\","
        "\"goal\":\"find docs\"}}";
    hu_hula_program_t prog;
    HU_ASSERT_EQ(hu_hula_parse_json(&alloc, json, strlen(json), &prog), HU_OK);

    hu_hula_exec_t exec;
    HU_ASSERT_EQ(hu_hula_exec_init(&exec, alloc, &prog, NULL, 0), HU_OK);
    HU_ASSERT_EQ(hu_hula_exec_run(&exec), HU_OK);

    const hu_hula_result_t *r = hu_hula_exec_result(&exec, "d1");
    HU_ASSERT_NOT_NULL(r);
    HU_ASSERT_EQ(r->status, HU_HULA_DONE);
    HU_ASSERT_STR_EQ(r->output, "find docs");

    hu_hula_exec_deinit(&exec);
    hu_hula_program_deinit(&prog);
}

static void hula_exec_emit_static_value(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *json =
        "{\"name\":\"out\",\"root\":{\"op\":\"emit\",\"id\":\"e1\","
        "\"emit_key\":\"result\",\"emit_value\":\"completed\"}}";
    hu_hula_program_t prog;
    HU_ASSERT_EQ(hu_hula_parse_json(&alloc, json, strlen(json), &prog), HU_OK);

    hu_hula_exec_t exec;
    HU_ASSERT_EQ(hu_hula_exec_init(&exec, alloc, &prog, NULL, 0), HU_OK);
    HU_ASSERT_EQ(hu_hula_exec_run(&exec), HU_OK);

    const hu_hula_result_t *r = hu_hula_exec_result(&exec, "e1");
    HU_ASSERT_NOT_NULL(r);
    HU_ASSERT_EQ(r->status, HU_HULA_DONE);
    HU_ASSERT_STR_EQ(r->output, "completed");

    hu_hula_exec_deinit(&exec);
    hu_hula_program_deinit(&prog);
}

static void hula_exec_call_unknown_tool(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *json =
        "{\"name\":\"bad\",\"root\":{\"op\":\"call\",\"id\":\"c1\","
        "\"tool\":\"nonexistent\",\"args\":{}}}";
    hu_hula_program_t prog;
    HU_ASSERT_EQ(hu_hula_parse_json(&alloc, json, strlen(json), &prog), HU_OK);

    hu_tool_t tools[2];
    make_tools(tools);
    hu_hula_exec_t exec;
    HU_ASSERT_EQ(hu_hula_exec_init(&exec, alloc, &prog, tools, 2), HU_OK);
    HU_ASSERT_EQ(hu_hula_exec_run(&exec), HU_OK);

    HU_ASSERT_EQ(hu_hula_exec_result(&exec, "c1")->status, HU_HULA_FAILED);

    hu_hula_exec_deinit(&exec);
    hu_hula_program_deinit(&prog);
}

/* ── Round-trip (parse → serialize → parse) ─────────────────────────────── */

static void hula_roundtrip_preserves_structure(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *json =
        "{\"name\":\"rt\",\"version\":1,"
        "\"root\":{\"op\":\"seq\",\"id\":\"s1\","
        "\"children\":["
        "{\"op\":\"call\",\"id\":\"c1\",\"tool\":\"echo\",\"args\":{\"text\":\"x\"}}"
        "]}}";
    hu_hula_program_t prog;
    HU_ASSERT_EQ(hu_hula_parse_json(&alloc, json, strlen(json), &prog), HU_OK);

    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(hu_hula_to_json(&alloc, &prog, &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_GT(out_len, 0);

    hu_hula_program_t prog2;
    HU_ASSERT_EQ(hu_hula_parse_json(&alloc, out, out_len, &prog2), HU_OK);
    HU_ASSERT_STR_EQ(prog2.name, "rt");
    HU_ASSERT_EQ(prog2.root->op, HU_HULA_SEQ);
    HU_ASSERT_EQ(prog2.root->children_count, 1u);
    HU_ASSERT_STR_EQ(prog2.root->children[0]->tool_name, "echo");

    hu_str_free(&alloc, out);
    hu_hula_program_deinit(&prog2);
    hu_hula_program_deinit(&prog);
}

static void hula_exec_emit_resolves_ref(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *json =
        "{\"name\":\"ref\",\"root\":{\"op\":\"seq\",\"id\":\"s1\","
        "\"children\":["
        "{\"op\":\"call\",\"id\":\"c1\",\"tool\":\"echo\",\"args\":{\"text\":\"data\"}},"
        "{\"op\":\"emit\",\"id\":\"e1\",\"emit_key\":\"out\",\"emit_value\":\"$c1\"}"
        "]}}";
    hu_hula_program_t prog;
    HU_ASSERT_EQ(hu_hula_parse_json(&alloc, json, strlen(json), &prog), HU_OK);

    hu_tool_t tools[2];
    make_tools(tools);
    hu_hula_exec_t exec;
    HU_ASSERT_EQ(hu_hula_exec_init(&exec, alloc, &prog, tools, 2), HU_OK);
    HU_ASSERT_EQ(hu_hula_exec_run(&exec), HU_OK);

    const hu_hula_result_t *r = hu_hula_exec_result(&exec, "e1");
    HU_ASSERT_NOT_NULL(r);
    HU_ASSERT_EQ(r->status, HU_HULA_DONE);
    HU_ASSERT_STR_EQ(r->output, "data");

    hu_hula_exec_deinit(&exec);
    hu_hula_program_deinit(&prog);
}

static void hula_exec_nested_seq_par(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *json =
        "{\"name\":\"nested\",\"root\":{\"op\":\"seq\",\"id\":\"s1\","
        "\"children\":["
        "{\"op\":\"par\",\"id\":\"p1\",\"children\":["
        "{\"op\":\"call\",\"id\":\"c1\",\"tool\":\"echo\",\"args\":{\"text\":\"a\"}},"
        "{\"op\":\"call\",\"id\":\"c2\",\"tool\":\"echo\",\"args\":{\"text\":\"b\"}}"
        "]},"
        "{\"op\":\"call\",\"id\":\"c3\",\"tool\":\"echo\",\"args\":{\"text\":\"final\"}}"
        "]}}";
    hu_hula_program_t prog;
    HU_ASSERT_EQ(hu_hula_parse_json(&alloc, json, strlen(json), &prog), HU_OK);

    hu_tool_t tools[2];
    make_tools(tools);
    hu_hula_exec_t exec;
    HU_ASSERT_EQ(hu_hula_exec_init(&exec, alloc, &prog, tools, 2), HU_OK);
    HU_ASSERT_EQ(hu_hula_exec_run(&exec), HU_OK);

    HU_ASSERT_EQ(hu_hula_exec_result(&exec, "c1")->status, HU_HULA_DONE);
    HU_ASSERT_EQ(hu_hula_exec_result(&exec, "c2")->status, HU_HULA_DONE);
    HU_ASSERT_EQ(hu_hula_exec_result(&exec, "c3")->status, HU_HULA_DONE);
    HU_ASSERT_STR_EQ(hu_hula_exec_result(&exec, "s1")->output, "final");

    hu_hula_exec_deinit(&exec);
    hu_hula_program_deinit(&prog);
}

void run_hula_tests(void) {
    HU_TEST_SUITE("hula");

    HU_RUN_TEST(hula_op_names_correct);
    HU_RUN_TEST(hula_pred_names_correct);
    HU_RUN_TEST(hula_status_names_correct);

    HU_RUN_TEST(hula_program_init_sets_name);
    HU_RUN_TEST(hula_program_alloc_node_returns_node);
    HU_RUN_TEST(hula_program_alloc_node_respects_max);

    HU_RUN_TEST(hula_parse_simple_call);
    HU_RUN_TEST(hula_parse_seq_with_children);
    HU_RUN_TEST(hula_parse_branch);
    HU_RUN_TEST(hula_parse_loop);
    HU_RUN_TEST(hula_parse_delegate);
    HU_RUN_TEST(hula_parse_emit);
    HU_RUN_TEST(hula_parse_invalid_json_returns_error);
    HU_RUN_TEST(hula_parse_missing_root_returns_error);

    HU_RUN_TEST(hula_validate_valid_call);
    HU_RUN_TEST(hula_validate_unknown_tool);
    HU_RUN_TEST(hula_validate_call_missing_tool);
    HU_RUN_TEST(hula_validate_seq_no_children);
    HU_RUN_TEST(hula_validate_no_root);
    HU_RUN_TEST(hula_validate_delegate_missing_goal);
    HU_RUN_TEST(hula_validate_emit_missing_key);

    HU_RUN_TEST(hula_exec_simple_call);
    HU_RUN_TEST(hula_exec_seq_propagates_output);
    HU_RUN_TEST(hula_exec_seq_short_circuits_on_failure);
    HU_RUN_TEST(hula_exec_par_runs_all);
    HU_RUN_TEST(hula_exec_branch_takes_then_on_success);
    HU_RUN_TEST(hula_exec_branch_takes_else_on_failure);
    HU_RUN_TEST(hula_exec_loop_bounded);
    HU_RUN_TEST(hula_exec_delegate_stub);
    HU_RUN_TEST(hula_exec_emit_static_value);
    HU_RUN_TEST(hula_exec_call_unknown_tool);
    HU_RUN_TEST(hula_roundtrip_preserves_structure);
    HU_RUN_TEST(hula_exec_emit_resolves_ref);
    HU_RUN_TEST(hula_exec_nested_seq_par);
}
