#include "seaclaw/agent/dag.h"
#include "seaclaw/agent/dag_executor.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/string.h"
#include "test_framework.h"
#include <string.h>

static void dag_add_node_and_find(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_dag_t dag;
    sc_dag_init(&dag, alloc);

    const char *deps[] = {"t0"};
    sc_error_t err = sc_dag_add_node(&dag, "t1", "web_search", "{\"q\":\"x\"}", deps, 1);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(dag.node_count, 1);

    sc_dag_node_t *n = sc_dag_find_node(&dag, "t1", 2);
    SC_ASSERT_NOT_NULL(n);
    SC_ASSERT_STR_EQ(n->id, "t1");
    SC_ASSERT_STR_EQ(n->tool_name, "web_search");
    SC_ASSERT_STR_EQ(n->args_json, "{\"q\":\"x\"}");
    SC_ASSERT_EQ(n->dep_count, 1);
    SC_ASSERT_STR_EQ(n->deps[0], "t0");
    SC_ASSERT_EQ(n->status, SC_DAG_PENDING);

    sc_dag_deinit(&dag);
}

static void dag_validate_accepts_valid_dag(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_dag_t dag;
    sc_dag_init(&dag, alloc);

    sc_dag_add_node(&dag, "t0", "shell", "{}", NULL, 0);
    sc_dag_add_node(&dag, "t1", "web_search", "{}", (const char *[]){"t0"}, 1);
    sc_dag_add_node(&dag, "t2", "file_read", "{}", (const char *[]){"t0"}, 1);

    sc_error_t err = sc_dag_validate(&dag);
    SC_ASSERT_EQ(err, SC_OK);

    sc_dag_deinit(&dag);
}

static void dag_validate_detects_cycle(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_dag_t dag;
    sc_dag_init(&dag, alloc);

    sc_dag_add_node(&dag, "t0", "a", "{}", (const char *[]){"t2"}, 1);
    sc_dag_add_node(&dag, "t1", "b", "{}", (const char *[]){"t0"}, 1);
    sc_dag_add_node(&dag, "t2", "c", "{}", (const char *[]){"t1"}, 1);

    sc_error_t err = sc_dag_validate(&dag);
    SC_ASSERT_NEQ(err, SC_OK);

    sc_dag_deinit(&dag);
}

static void dag_validate_detects_missing_dep(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_dag_t dag;
    sc_dag_init(&dag, alloc);

    sc_dag_add_node(&dag, "t0", "a", "{}", NULL, 0);
    sc_dag_add_node(&dag, "t1", "b", "{}", (const char *[]){"t99"}, 1);

    sc_error_t err = sc_dag_validate(&dag);
    SC_ASSERT_NEQ(err, SC_OK);

    sc_dag_deinit(&dag);
}

static void dag_validate_detects_duplicate_id(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_dag_t dag;
    sc_dag_init(&dag, alloc);

    sc_dag_add_node(&dag, "t0", "a", "{}", NULL, 0);
    sc_dag_add_node(&dag, "t0", "b", "{}", NULL, 0);

    sc_error_t err = sc_dag_validate(&dag);
    SC_ASSERT_NEQ(err, SC_OK);

    sc_dag_deinit(&dag);
}

static void dag_parse_json_creates_nodes(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_dag_t dag;
    sc_dag_init(&dag, alloc);

    const char *json =
        "{\"tasks\":[{\"id\":\"t1\",\"tool\":\"web_search\",\"args\":{\"q\":\"x\"},\"deps\":[]},"
        "{\"id\":\"t2\",\"tool\":\"file_read\",\"args\":{},\"deps\":[\"t1\"]}]}";
    sc_error_t err = sc_dag_parse_json(&dag, &alloc, json, strlen(json));
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(dag.node_count, 2);

    sc_dag_node_t *n1 = sc_dag_find_node(&dag, "t1", 2);
    SC_ASSERT_NOT_NULL(n1);
    SC_ASSERT_STR_EQ(n1->tool_name, "web_search");

    sc_dag_node_t *n2 = sc_dag_find_node(&dag, "t2", 2);
    SC_ASSERT_NOT_NULL(n2);
    SC_ASSERT_STR_EQ(n2->tool_name, "file_read");
    SC_ASSERT_EQ(n2->dep_count, 1);
    SC_ASSERT_STR_EQ(n2->deps[0], "t1");

    sc_dag_deinit(&dag);
}

static void dag_is_complete_when_all_done(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_dag_t dag;
    sc_dag_init(&dag, alloc);

    sc_dag_add_node(&dag, "t0", "a", "{}", NULL, 0);
    sc_dag_add_node(&dag, "t1", "b", "{}", (const char *[]){"t0"}, 1);

    SC_ASSERT_FALSE(sc_dag_is_complete(&dag));

    dag.nodes[0].status = SC_DAG_DONE;
    SC_ASSERT_FALSE(sc_dag_is_complete(&dag));

    dag.nodes[1].status = SC_DAG_DONE;
    SC_ASSERT_TRUE(sc_dag_is_complete(&dag));

    dag.nodes[1].status = SC_DAG_FAILED;
    SC_ASSERT_TRUE(sc_dag_is_complete(&dag));

    sc_dag_deinit(&dag);
}

static void dag_next_batch_returns_roots_first(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_dag_t dag;
    sc_dag_init(&dag, alloc);

    sc_dag_add_node(&dag, "t0", "a", "{}", NULL, 0);
    sc_dag_add_node(&dag, "t1", "b", "{}", NULL, 0);
    sc_dag_add_node(&dag, "t2", "c", "{}", (const char *[]){"t0", "t1"}, 2);

    sc_dag_batch_t batch;
    sc_error_t err = sc_dag_next_batch(&dag, &batch);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(batch.count, 2);

    sc_dag_deinit(&dag);
}

static void dag_next_batch_returns_dependents_after_roots_done(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_dag_t dag;
    sc_dag_init(&dag, alloc);

    sc_dag_add_node(&dag, "t0", "a", "{}", NULL, 0);
    sc_dag_add_node(&dag, "t1", "b", "{}", (const char *[]){"t0"}, 1);

    sc_dag_batch_t batch;
    sc_dag_next_batch(&dag, &batch);
    SC_ASSERT_EQ(batch.count, 1);
    batch.nodes[0]->status = SC_DAG_DONE;
    batch.nodes[0]->result = sc_strdup(&dag.alloc, "done");
    batch.nodes[0]->result_len = 4;

    sc_dag_next_batch(&dag, &batch);
    SC_ASSERT_EQ(batch.count, 1);
    SC_ASSERT_STR_EQ(batch.nodes[0]->id, "t1");

    sc_dag_deinit(&dag);
}

static void dag_resolve_vars_substitutes(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_dag_t dag;
    sc_dag_init(&dag, alloc);

    sc_dag_add_node(&dag, "t1", "a", "{}", NULL, 0);
    dag.nodes[0].status = SC_DAG_DONE;
    dag.nodes[0].result = sc_strdup(&alloc, "hello");
    dag.nodes[0].result_len = 5;

    char *resolved = NULL;
    size_t resolved_len = 0;
    const char *args = "prefix $t1 suffix";
    sc_error_t err = sc_dag_resolve_vars(&alloc, &dag, args, strlen(args), &resolved, &resolved_len);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(resolved);
    SC_ASSERT_STR_EQ(resolved, "prefix hello suffix");
    sc_str_free(&alloc, resolved);

    sc_dag_deinit(&dag);
}

static void dag_resolve_vars_no_refs(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_dag_t dag;
    sc_dag_init(&dag, alloc);

    sc_dag_add_node(&dag, "t1", "a", "{}", NULL, 0);

    char *resolved = NULL;
    size_t resolved_len = 0;
    const char *args = "no vars here";
    sc_error_t err = sc_dag_resolve_vars(&alloc, &dag, args, strlen(args), &resolved, &resolved_len);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(resolved);
    SC_ASSERT_STR_EQ(resolved, "no vars here");
    sc_str_free(&alloc, resolved);

    sc_dag_deinit(&dag);
}

void run_dag_tests(void) {
    SC_TEST_SUITE("dag");
    SC_RUN_TEST(dag_add_node_and_find);
    SC_RUN_TEST(dag_validate_accepts_valid_dag);
    SC_RUN_TEST(dag_validate_detects_cycle);
    SC_RUN_TEST(dag_validate_detects_missing_dep);
    SC_RUN_TEST(dag_validate_detects_duplicate_id);
    SC_RUN_TEST(dag_parse_json_creates_nodes);
    SC_RUN_TEST(dag_is_complete_when_all_done);
    SC_RUN_TEST(dag_next_batch_returns_roots_first);
    SC_RUN_TEST(dag_next_batch_returns_dependents_after_roots_done);
    SC_RUN_TEST(dag_resolve_vars_substitutes);
    SC_RUN_TEST(dag_resolve_vars_no_refs);
}
