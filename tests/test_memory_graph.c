/*
 * AGI-S5 Multi-Graph Memory (MAGMA) tests.
 */
#include "human/core/allocator.h"
#include "human/memory/memory_graph.h"
#include "test_framework.h"
#include <string.h>

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>

static void graph_memory_init_tables(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    int rc = sqlite3_open(":memory:", &db);
    HU_ASSERT_EQ(rc, SQLITE_OK);
    HU_ASSERT_NOT_NULL(db);

    hu_memory_graph_t g = {0};
    hu_error_t err = hu_memory_graph_create(&alloc, db, &g);
    HU_ASSERT_EQ(err, HU_OK);
    err = hu_memory_graph_init_tables(&g);
    HU_ASSERT_EQ(err, HU_OK);

    hu_memory_graph_deinit(&g);
    sqlite3_close(db);
}

static void graph_memory_add_node_returns_id(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    sqlite3_open(":memory:", &db);

    hu_memory_graph_t g = {0};
    hu_memory_graph_create(&alloc, db, &g);

    int64_t id = 0;
    hu_error_t err = hu_memory_graph_add_node(&g, "test content", 12, &id);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_GT(id, 0);

    hu_memory_graph_deinit(&g);
    sqlite3_close(db);
}

static void graph_memory_add_edge_creates_link(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    sqlite3_open(":memory:", &db);

    hu_memory_graph_t g = {0};
    hu_memory_graph_create(&alloc, db, &g);

    int64_t id_a = 0, id_b = 0;
    hu_memory_graph_add_node(&g, "node A", 6, &id_a);
    hu_memory_graph_add_node(&g, "node B", 6, &id_b);

    hu_error_t err =
        hu_memory_graph_add_edge(&g, id_a, id_b, HU_GRAPH_SEMANTIC, 1.0);
    HU_ASSERT_EQ(err, HU_OK);

    hu_memory_graph_deinit(&g);
    sqlite3_close(db);
}

static void graph_memory_traverse_single_hop(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    sqlite3_open(":memory:", &db);

    hu_memory_graph_t g = {0};
    hu_memory_graph_create(&alloc, db, &g);

    int64_t id_a = 0, id_b = 0;
    hu_memory_graph_add_node(&g, "A", 1, &id_a);
    hu_memory_graph_add_node(&g, "B", 1, &id_b);
    hu_memory_graph_add_edge(&g, id_a, id_b, HU_GRAPH_SEMANTIC, 1.0);

    hu_memory_node_t results[8];
    size_t count = 0;
    hu_error_t err =
        hu_memory_graph_traverse(&g, id_a, HU_GRAPH_SEMANTIC, 1, results, 8, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, 1);
    HU_ASSERT_EQ(results[0].id, id_b);

    hu_memory_graph_deinit(&g);
    sqlite3_close(db);
}

static void graph_memory_traverse_multi_hop(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    sqlite3_open(":memory:", &db);

    hu_memory_graph_t g = {0};
    hu_memory_graph_create(&alloc, db, &g);

    int64_t id_a = 0, id_b = 0, id_c = 0;
    hu_memory_graph_add_node(&g, "A", 1, &id_a);
    hu_memory_graph_add_node(&g, "B", 1, &id_b);
    hu_memory_graph_add_node(&g, "C", 1, &id_c);
    hu_memory_graph_add_edge(&g, id_a, id_b, HU_GRAPH_SEMANTIC, 1.0);
    hu_memory_graph_add_edge(&g, id_b, id_c, HU_GRAPH_SEMANTIC, 1.0);

    hu_memory_node_t results[8];
    size_t count = 0;
    hu_error_t err =
        hu_memory_graph_traverse(&g, id_a, HU_GRAPH_SEMANTIC, 2, results, 8, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, 2);

    bool has_b = false, has_c = false;
    for (size_t i = 0; i < count; i++) {
        if (results[i].id == id_b)
            has_b = true;
        if (results[i].id == id_c)
            has_c = true;
    }
    HU_ASSERT_TRUE(has_b);
    HU_ASSERT_TRUE(has_c);

    hu_memory_graph_deinit(&g);
    sqlite3_close(db);
}

static void graph_memory_traverse_filters_by_type(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    sqlite3_open(":memory:", &db);

    hu_memory_graph_t g = {0};
    hu_memory_graph_create(&alloc, db, &g);

    int64_t id_a = 0, id_b = 0, id_c = 0;
    hu_memory_graph_add_node(&g, "A", 1, &id_a);
    hu_memory_graph_add_node(&g, "B", 1, &id_b);
    hu_memory_graph_add_node(&g, "C", 1, &id_c);
    hu_memory_graph_add_edge(&g, id_a, id_b, HU_GRAPH_SEMANTIC, 1.0);
    hu_memory_graph_add_edge(&g, id_a, id_c, HU_GRAPH_TEMPORAL, 1.0);

    hu_memory_node_t results[8];
    size_t count = 0;
    hu_error_t err =
        hu_memory_graph_traverse(&g, id_a, HU_GRAPH_SEMANTIC, 1, results, 8, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, 1);
    HU_ASSERT_EQ(results[0].id, id_b);

    hu_memory_graph_deinit(&g);
    sqlite3_close(db);
}

static void graph_memory_find_bridges(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    sqlite3_open(":memory:", &db);

    hu_memory_graph_t g = {0};
    hu_memory_graph_create(&alloc, db, &g);

    int64_t id_a = 0, id_b = 0, id_x = 0;
    hu_memory_graph_add_node(&g, "A", 1, &id_a);
    hu_memory_graph_add_node(&g, "B", 1, &id_b);
    hu_memory_graph_add_node(&g, "X", 1, &id_x);
    hu_memory_graph_add_edge(&g, id_a, id_x, HU_GRAPH_SEMANTIC, 1.0);
    hu_memory_graph_add_edge(&g, id_b, id_x, HU_GRAPH_SEMANTIC, 1.0);

    hu_memory_node_t bridges[8];
    size_t count = 0;
    hu_error_t err =
        hu_memory_graph_find_bridges(&g, id_a, id_b, bridges, 8, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, 1);
    HU_ASSERT_EQ(bridges[0].id, id_x);

    hu_memory_graph_deinit(&g);
    sqlite3_close(db);
}

static void graph_memory_type_name_correct(void) {
    HU_ASSERT_STR_EQ(hu_memory_graph_type_name(HU_GRAPH_SEMANTIC), "semantic");
    HU_ASSERT_STR_EQ(hu_memory_graph_type_name(HU_GRAPH_TEMPORAL), "temporal");
    HU_ASSERT_STR_EQ(hu_memory_graph_type_name(HU_GRAPH_ENTITY), "entity");
    HU_ASSERT_STR_EQ(hu_memory_graph_type_name(HU_GRAPH_CAUSAL), "causal");
}

static void graph_memory_null_args_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    sqlite3_open(":memory:", &db);

    hu_memory_graph_t g = {0};
    hu_memory_graph_create(&alloc, db, &g);

    int64_t id = 0;
    hu_error_t err = hu_memory_graph_add_node(NULL, "x", 1, &id);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);

    err = hu_memory_graph_add_node(&g, "x", 1, NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);

    hu_memory_node_t results[4];
    size_t count = 0;
    err = hu_memory_graph_traverse(NULL, 1, HU_GRAPH_SEMANTIC, 1, results, 4, &count);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);

    hu_memory_graph_deinit(&g);
    sqlite3_close(db);
}

static void graph_memory_ingest_creates_temporal_edges(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    sqlite3_open(":memory:", &db);

    hu_memory_graph_t g = {0};
    hu_memory_graph_create(&alloc, db, &g);

    int64_t ts = 1000000;
    hu_error_t err = hu_memory_graph_ingest(&g, "first memory", 12, ts);
    HU_ASSERT_EQ(err, HU_OK);
    err = hu_memory_graph_ingest(&g, "second memory", 13, ts + 1000);
    HU_ASSERT_EQ(err, HU_OK);

    hu_memory_node_t results[8];
    size_t count = 0;
    err = hu_memory_graph_traverse(&g, 2, HU_GRAPH_TEMPORAL, 1, results, 8, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(count >= 1);

    hu_memory_graph_deinit(&g);
    sqlite3_close(db);
}

static void graph_memory_build_context_returns_text(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    sqlite3_open(":memory:", &db);

    hu_memory_graph_t g = {0};
    hu_memory_graph_create(&alloc, db, &g);

    int64_t id_a = 0, id_b = 0;
    hu_memory_graph_add_node(&g, "node about cats", 15, &id_a);
    hu_memory_graph_add_node(&g, "node about dogs", 15, &id_b);
    hu_memory_graph_add_edge(&g, id_a, id_b, HU_GRAPH_SEMANTIC, 1.0);

    char *ctx = NULL;
    size_t ctx_len = 0;
    hu_error_t err = hu_memory_graph_build_context(&g, &alloc, id_a, 1, &ctx, &ctx_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(ctx);
    HU_ASSERT_TRUE(ctx_len > 0);
    HU_ASSERT_TRUE(strstr(ctx, "semantic") != NULL || strstr(ctx, "dogs") != NULL);

    alloc.free(alloc.ctx, ctx, ctx_len + 1);
    hu_memory_graph_deinit(&g);
    sqlite3_close(db);
}

static void graph_memory_ingest_null_args(void) {
    hu_error_t err = hu_memory_graph_ingest(NULL, "x", 1, 0);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);

    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    sqlite3_open(":memory:", &db);
    hu_memory_graph_t g = {0};
    hu_memory_graph_create(&alloc, db, &g);

    err = hu_memory_graph_ingest(&g, NULL, 0, 0);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);

    hu_memory_graph_deinit(&g);
    sqlite3_close(db);
}

void run_memory_graph_tests(void) {
    HU_TEST_SUITE("memory_graph");
    HU_RUN_TEST(graph_memory_init_tables);
    HU_RUN_TEST(graph_memory_add_node_returns_id);
    HU_RUN_TEST(graph_memory_add_edge_creates_link);
    HU_RUN_TEST(graph_memory_traverse_single_hop);
    HU_RUN_TEST(graph_memory_traverse_multi_hop);
    HU_RUN_TEST(graph_memory_traverse_filters_by_type);
    HU_RUN_TEST(graph_memory_find_bridges);
    HU_RUN_TEST(graph_memory_type_name_correct);
    HU_RUN_TEST(graph_memory_null_args_returns_error);
    HU_RUN_TEST(graph_memory_ingest_creates_temporal_edges);
    HU_RUN_TEST(graph_memory_build_context_returns_text);
    HU_RUN_TEST(graph_memory_ingest_null_args);
}

#else

void run_memory_graph_tests(void) {
    HU_TEST_SUITE("memory_graph");
    /* No tests when SQLite disabled */
}

#endif /* HU_ENABLE_SQLITE */
