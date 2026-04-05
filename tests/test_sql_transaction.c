#ifdef HU_ENABLE_SQLITE

#include "test_framework.h"
#include "human/memory/sql_transaction.h"
#include <sqlite3.h>

static void test_txn_begin_commit(void) {
    sqlite3 *db;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
    hu_sql_txn_t txn = {0};
    HU_ASSERT_EQ(hu_sql_txn_begin(&txn, db), HU_OK);
    HU_ASSERT_TRUE(txn.active);
    HU_ASSERT_EQ(hu_sql_txn_commit(&txn), HU_OK);
    HU_ASSERT_TRUE(!txn.active);
    sqlite3_close(db);
}

static void test_txn_begin_rollback(void) {
    sqlite3 *db;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
    hu_sql_txn_t txn = {0};
    HU_ASSERT_EQ(hu_sql_txn_begin(&txn, db), HU_OK);
    hu_sql_txn_rollback(&txn);
    HU_ASSERT_TRUE(!txn.active);
    sqlite3_close(db);
}

static void test_txn_rollback_inactive_noop(void) {
    hu_sql_txn_t txn = {0};
    hu_sql_txn_rollback(&txn); /* should not crash */
    HU_ASSERT_TRUE(!txn.active);
}

static void test_txn_begin_null_args(void) {
    sqlite3 *db;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
    hu_sql_txn_t txn = {0};
    HU_ASSERT_EQ(hu_sql_txn_begin(NULL, db), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_sql_txn_begin(&txn, NULL), HU_ERR_INVALID_ARGUMENT);
    sqlite3_close(db);
}

static void test_txn_commit_inactive_fails(void) {
    hu_sql_txn_t txn = {0};
    HU_ASSERT_EQ(hu_sql_txn_commit(&txn), HU_ERR_INVALID_ARGUMENT);
}

static void test_txn_data_persists_after_commit(void) {
    sqlite3 *db;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
    sqlite3_exec(db, "CREATE TABLE t(v TEXT)", NULL, NULL, NULL);

    hu_sql_txn_t txn = {0};
    HU_ASSERT_EQ(hu_sql_txn_begin(&txn, db), HU_OK);
    sqlite3_exec(db, "INSERT INTO t VALUES('hello')", NULL, NULL, NULL);
    HU_ASSERT_EQ(hu_sql_txn_commit(&txn), HU_OK);

    sqlite3_stmt *sel;
    sqlite3_prepare_v2(db, "SELECT v FROM t", -1, &sel, NULL);
    HU_ASSERT_EQ(sqlite3_step(sel), SQLITE_ROW);
    HU_ASSERT_STR_EQ((const char *)sqlite3_column_text(sel, 0), "hello");
    sqlite3_finalize(sel);
    sqlite3_close(db);
}

static void test_txn_data_reverts_after_rollback(void) {
    sqlite3 *db;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
    sqlite3_exec(db, "CREATE TABLE t(v TEXT)", NULL, NULL, NULL);

    hu_sql_txn_t txn = {0};
    HU_ASSERT_EQ(hu_sql_txn_begin(&txn, db), HU_OK);
    sqlite3_exec(db, "INSERT INTO t VALUES('bye')", NULL, NULL, NULL);
    hu_sql_txn_rollback(&txn);

    sqlite3_stmt *sel;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM t", -1, &sel, NULL);
    HU_ASSERT_EQ(sqlite3_step(sel), SQLITE_ROW);
    HU_ASSERT_EQ(sqlite3_column_int(sel, 0), 0);
    sqlite3_finalize(sel);
    sqlite3_close(db);
}

void run_sql_transaction_tests(void) {
    HU_TEST_SUITE("SQL Transaction Helper");
    HU_RUN_TEST(test_txn_begin_commit);
    HU_RUN_TEST(test_txn_begin_rollback);
    HU_RUN_TEST(test_txn_rollback_inactive_noop);
    HU_RUN_TEST(test_txn_begin_null_args);
    HU_RUN_TEST(test_txn_commit_inactive_fails);
    HU_RUN_TEST(test_txn_data_persists_after_commit);
    HU_RUN_TEST(test_txn_data_reverts_after_rollback);
}

#else

typedef int hu_sql_transaction_test_unused_;

void run_sql_transaction_tests(void) {
    (void)0;
}

#endif /* HU_ENABLE_SQLITE */
