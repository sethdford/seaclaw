#include "human/core/allocator.h"
#include "human/tools/canvas_store.h"
#include "test_framework.h"
#include <string.h>

static void test_canvas_persist_set_db_null(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_canvas_store_t *store = hu_canvas_store_create(&alloc);
    HU_ASSERT_EQ(hu_canvas_store_set_db(store, NULL), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_canvas_store_set_db(NULL, (void *)1), HU_ERR_INVALID_ARGUMENT);
    hu_canvas_store_destroy(store);
}

static void test_canvas_persist_save_null(void) {
    HU_ASSERT_EQ(hu_canvas_persist_save(NULL, "id", "html", NULL, NULL, NULL, "x"),
                 HU_ERR_INVALID_ARGUMENT);
}

static void test_canvas_persist_delete_null(void) {
    HU_ASSERT_EQ(hu_canvas_persist_delete(NULL, "id"), HU_ERR_INVALID_ARGUMENT);
}

static void test_canvas_persist_load_null(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_canvas_store_t *store = hu_canvas_store_create(&alloc);
    HU_ASSERT_EQ(hu_canvas_persist_load_all(NULL, store), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_canvas_persist_load_all((void *)1, NULL), HU_ERR_INVALID_ARGUMENT);
    hu_canvas_store_destroy(store);
}

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>

static void test_canvas_persist_sqlite_roundtrip(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);

    hu_canvas_store_t *store = hu_canvas_store_create(&alloc);
    HU_ASSERT_EQ(hu_canvas_store_set_db(store, db), HU_OK);

    HU_ASSERT_EQ(hu_canvas_store_put_canvas(store, "cv_a", "html", NULL, NULL, "Alpha",
                                            "<h1>Hello</h1>"),
                 HU_OK);
    HU_ASSERT_EQ(hu_canvas_store_put_canvas(store, "cv_b", "react", "{\"react\":\"18\"}", "jsx",
                                            "Counter", "export default () => <p>0</p>"),
                 HU_OK);

    hu_canvas_store_agent_update(store, "cv_a", "<h1>Updated</h1>");

    hu_canvas_store_destroy(store);

    hu_canvas_store_t *store2 = hu_canvas_store_create(&alloc);
    HU_ASSERT_EQ(hu_canvas_persist_load_all(db, store2), HU_OK);
    HU_ASSERT_EQ(hu_canvas_store_count(store2), 2u);

    hu_canvas_info_t info_a;
    HU_ASSERT_TRUE(hu_canvas_store_find(store2, "cv_a", &info_a));
    HU_ASSERT_STR_EQ(info_a.format, "html");
    HU_ASSERT_STR_EQ(info_a.title, "Alpha");
    HU_ASSERT_STR_EQ(info_a.content, "<h1>Updated</h1>");

    hu_canvas_info_t info_b;
    HU_ASSERT_TRUE(hu_canvas_store_find(store2, "cv_b", &info_b));
    HU_ASSERT_STR_EQ(info_b.format, "react");
    HU_ASSERT_STR_EQ(info_b.language, "jsx");
    HU_ASSERT(strstr(info_b.imports, "react") != NULL);
    HU_ASSERT(strstr(info_b.content, "export") != NULL);

    hu_canvas_store_destroy(store2);
    sqlite3_close(db);
}

static void test_canvas_persist_sqlite_delete(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    sqlite3_open(":memory:", &db);

    hu_canvas_store_t *store = hu_canvas_store_create(&alloc);
    hu_canvas_store_set_db(store, db);
    hu_canvas_store_put_canvas(store, "cv_del", "html", NULL, NULL, "Del", "content");
    HU_ASSERT_EQ(hu_canvas_store_count(store), 1u);

    hu_canvas_store_remove_canvas(store, "cv_del");
    HU_ASSERT_EQ(hu_canvas_store_count(store), 0u);

    hu_canvas_store_destroy(store);

    hu_canvas_store_t *store2 = hu_canvas_store_create(&alloc);
    hu_canvas_persist_load_all(db, store2);
    HU_ASSERT_EQ(hu_canvas_store_count(store2), 0u);

    hu_canvas_store_destroy(store2);
    sqlite3_close(db);
}

static void test_canvas_persist_version_roundtrip(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    sqlite3_open(":memory:", &db);

    hu_canvas_store_t *store = hu_canvas_store_create(&alloc);
    hu_canvas_store_set_db(store, db);
    hu_canvas_store_put_canvas(store, "cv_ver", "html", NULL, NULL, "Versioned", "v0");

    hu_canvas_store_agent_update(store, "cv_ver", "v1");
    hu_canvas_store_agent_update(store, "cv_ver", "v2");
    hu_canvas_store_agent_update(store, "cv_ver", "v3");

    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT COUNT(*) FROM canvas_versions WHERE canvas_id = 'cv_ver'";
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_step(stmt);
    int ver_count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    HU_ASSERT(ver_count >= 3);

    hu_canvas_store_destroy(store);
    sqlite3_close(db);
}

#endif /* HU_ENABLE_SQLITE */

void run_canvas_persist_tests(void) {
    HU_TEST_SUITE("CanvasPersist");
    HU_RUN_TEST(test_canvas_persist_set_db_null);
    HU_RUN_TEST(test_canvas_persist_save_null);
    HU_RUN_TEST(test_canvas_persist_delete_null);
    HU_RUN_TEST(test_canvas_persist_load_null);
#ifdef HU_ENABLE_SQLITE
    HU_RUN_TEST(test_canvas_persist_sqlite_roundtrip);
    HU_RUN_TEST(test_canvas_persist_sqlite_delete);
    HU_RUN_TEST(test_canvas_persist_version_roundtrip);
#endif
}
