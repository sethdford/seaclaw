#include "test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(HU_ENABLE_SQLITE)
#include <sqlite3.h>

static void integ_sqlite_file_crud_and_wal(void) {
    char tpl[] = "/tmp/hu_integ_XXXXXX";
    int fd = mkstemp(tpl);
    HU_SKIP_IF(fd < 0, "mkstemp failed");
    (void)close(fd);

    sqlite3 *db = NULL;
    int rc = sqlite3_open(tpl, &db);
    HU_SKIP_IF(rc != SQLITE_OK || !db, "sqlite3_open failed");

    char *errmsg = NULL;
    rc = sqlite3_exec(db, "CREATE TABLE t(x INTEGER PRIMARY KEY, v TEXT);", NULL, NULL, &errmsg);
    if (errmsg) {
        sqlite3_free(errmsg);
        errmsg = NULL;
    }
    HU_ASSERT_EQ(rc, SQLITE_OK);

    rc = sqlite3_exec(db, "INSERT INTO t(v) VALUES ('hi');", NULL, NULL, &errmsg);
    if (errmsg) {
        sqlite3_free(errmsg);
        errmsg = NULL;
    }
    HU_ASSERT_EQ(rc, SQLITE_OK);

    sqlite3_stmt *st = NULL;
    rc = sqlite3_prepare_v2(db, "SELECT v FROM t WHERE x=1", -1, &st, NULL);
    HU_ASSERT_EQ(rc, SQLITE_OK);
    rc = sqlite3_step(st);
    HU_ASSERT_EQ(rc, SQLITE_ROW);
    const unsigned char *txt = sqlite3_column_text(st, 0);
    HU_ASSERT_NOT_NULL(txt);
    HU_ASSERT_STR_EQ((const char *)txt, "hi");
    sqlite3_finalize(st);

    rc = sqlite3_exec(db, "PRAGMA journal_mode=WAL;", NULL, NULL, &errmsg);
    if (errmsg) {
        sqlite3_free(errmsg);
        errmsg = NULL;
    }
    HU_ASSERT_EQ(rc, SQLITE_OK);

    sqlite3_close(db);
    (void)unlink(tpl);
}

static void integ_sqlite_busy_lock_behavior(void) {
    char tpl[] = "/tmp/hu_integ_busy_XXXXXX";
    int fd = mkstemp(tpl);
    HU_SKIP_IF(fd < 0, "mkstemp failed");
    (void)close(fd);

    sqlite3 *db1 = NULL;
    sqlite3 *db2 = NULL;
    HU_SKIP_IF(sqlite3_open(tpl, &db1) != SQLITE_OK || !db1, "sqlite3_open db1 failed");
    HU_SKIP_IF(sqlite3_open(tpl, &db2) != SQLITE_OK || !db2, "sqlite3_open db2 failed");

    char *errmsg = NULL;
    int rc = sqlite3_exec(db1, "CREATE TABLE t_lock(x INTEGER PRIMARY KEY, v TEXT);", NULL, NULL,
                          &errmsg);
    if (errmsg) {
        sqlite3_free(errmsg);
        errmsg = NULL;
    }
    HU_ASSERT_EQ(rc, SQLITE_OK);

    rc = sqlite3_busy_timeout(db2, 0);
    HU_ASSERT_EQ(rc, SQLITE_OK);

    rc = sqlite3_exec(db1, "BEGIN EXCLUSIVE;", NULL, NULL, &errmsg);
    if (errmsg) {
        sqlite3_free(errmsg);
        errmsg = NULL;
    }
    HU_ASSERT_EQ(rc, SQLITE_OK);

    rc = sqlite3_exec(db2, "INSERT INTO t_lock(v) VALUES ('blocked');", NULL, NULL, &errmsg);
    if (errmsg) {
        sqlite3_free(errmsg);
        errmsg = NULL;
    }
    HU_ASSERT_EQ(rc, SQLITE_BUSY);

    rc = sqlite3_exec(db1, "COMMIT;", NULL, NULL, &errmsg);
    if (errmsg) {
        sqlite3_free(errmsg);
        errmsg = NULL;
    }
    HU_ASSERT_EQ(rc, SQLITE_OK);

    sqlite3_close(db2);
    sqlite3_close(db1);
    (void)unlink(tpl);
}
#else
static void integ_sqlite_disabled(void) {
    HU_SKIP_IF(1, "SQLite not enabled in this build");
}
#endif

void run_integration_sqlite_tests(void) {
#if defined(HU_ENABLE_SQLITE)
    HU_RUN_TEST(integ_sqlite_file_crud_and_wal);
    HU_RUN_TEST(integ_sqlite_busy_lock_behavior);
#else
    HU_RUN_TEST(integ_sqlite_disabled);
#endif
}
