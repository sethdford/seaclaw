/* Minimal chat.db fixture for iMessage SQL regression testing.
 * Creates an in-memory SQLite database with the iMessage schema and seed data,
 * then runs the same queries used by imessage.c to catch schema/column drift. */
#if HU_HAS_IMESSAGE && defined(HU_ENABLE_SQLITE)
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "test_framework.h"
#include <sqlite3.h>
#include <string.h>

static const char *schema_sql =
    "CREATE TABLE handle ("
    "  ROWID INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  id TEXT UNIQUE NOT NULL"
    ");"
    "CREATE TABLE message ("
    "  ROWID INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  guid TEXT UNIQUE,"
    "  text TEXT,"
    "  handle_id INTEGER,"
    "  date INTEGER DEFAULT 0,"
    "  is_from_me INTEGER DEFAULT 0,"
    "  associated_message_type INTEGER DEFAULT 0,"
    "  associated_message_guid TEXT,"
    "  attributedBody BLOB,"
    "  date_delivered INTEGER DEFAULT 0,"
    "  date_read INTEGER DEFAULT 0,"
    "  date_edited INTEGER DEFAULT 0,"
    "  date_retracted INTEGER DEFAULT 0,"
    "  thread_originator_guid TEXT,"
    "  balloon_bundle_id TEXT"
    ");"
    "CREATE TABLE attachment ("
    "  ROWID INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  filename TEXT"
    ");"
    "CREATE TABLE message_attachment_join ("
    "  message_id INTEGER,"
    "  attachment_id INTEGER"
    ");"
    "CREATE TABLE chat ("
    "  ROWID INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  guid TEXT UNIQUE"
    ");"
    "CREATE TABLE chat_message_join ("
    "  chat_id INTEGER,"
    "  message_id INTEGER"
    ");"
    "CREATE TABLE chat_handle_join ("
    "  chat_id INTEGER,"
    "  handle_id INTEGER"
    ");";

static const char *seed_sql =
    "INSERT INTO handle (id) VALUES ('+15559999999');"
    "INSERT INTO handle (id) VALUES ('user@example.com');"
    "INSERT INTO chat (guid) VALUES ('iMessage;-;+15559999999');"
    "INSERT INTO chat_handle_join (chat_id, handle_id) VALUES (1, 1);"
    /* Inbound text message from +15559999999 */
    "INSERT INTO message (guid, text, handle_id, date, is_from_me, associated_message_type)"
    "  VALUES ('MSG-001', 'Hello there', 1, 700000000000000000, 0, 0);"
    "INSERT INTO chat_message_join (chat_id, message_id) VALUES (1, 1);"
    /* Outbound message */
    "INSERT INTO message (guid, text, handle_id, date, is_from_me, associated_message_type)"
    "  VALUES ('MSG-002', 'Hi back', 1, 700000001000000000, 1, 0);"
    "INSERT INTO chat_message_join (chat_id, message_id) VALUES (1, 2);"
    /* Tapback on MSG-002 (heart=2000) */
    "INSERT INTO message (guid, text, handle_id, date, is_from_me,"
    "  associated_message_type, associated_message_guid)"
    "  VALUES ('MSG-003', NULL, 1, 700000002000000000, 0, 2000, 'MSG-002');"
    /* Message with attachment */
    "INSERT INTO message (guid, text, handle_id, date, is_from_me, associated_message_type)"
    "  VALUES ('MSG-004', NULL, 1, 700000003000000000, 0, 0);"
    "INSERT INTO attachment (filename) VALUES ('~/Library/Messages/photo.jpg');"
    "INSERT INTO message_attachment_join (message_id, attachment_id) VALUES (4, 1);"
    "INSERT INTO chat_message_join (chat_id, message_id) VALUES (1, 4);"
    /* Sticker message (balloon_bundle_id) */
    "INSERT INTO message (guid, text, handle_id, date, is_from_me,"
    "  associated_message_type, balloon_bundle_id)"
    "  VALUES ('MSG-005', NULL, 1, 700000004000000000, 0, 0,"
    "  'com.apple.messages.MSMessageExtensionBalloonPlugin:0000000000:com.apple.Stickers.UserGenerated.StickerPack');"
    "INSERT INTO chat_message_join (chat_id, message_id) VALUES (1, 5);";

static sqlite3 *open_fixture(void) {
    sqlite3 *db = NULL;
    int rc = sqlite3_open(":memory:", &db);
    if (rc != SQLITE_OK || !db)
        return NULL;
    char *err = NULL;
    rc = sqlite3_exec(db, schema_sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        sqlite3_free(err);
        sqlite3_close(db);
        return NULL;
    }
    rc = sqlite3_exec(db, seed_sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        sqlite3_free(err);
        sqlite3_close(db);
        return NULL;
    }
    return db;
}

static void test_chatdb_schema_creates_successfully(void) {
    sqlite3 *db = open_fixture();
    HU_ASSERT_NOT_NULL(db);
    sqlite3_close(db);
}

static void test_chatdb_poll_query_returns_inbound(void) {
    sqlite3 *db = open_fixture();
    HU_ASSERT_NOT_NULL(db);

    const char *sql =
        "SELECT m.ROWID, m.guid, m.text, h.id, m.attributedBody, m.balloon_bundle_id "
        "FROM message m "
        "JOIN handle h ON m.handle_id = h.ROWID "
        "WHERE m.is_from_me = 0 AND m.associated_message_type = 0 "
        "AND m.ROWID > ? "
        "AND ((m.text IS NOT NULL AND LENGTH(m.text) > 0) "
        "     OR (m.attributedBody IS NOT NULL AND LENGTH(m.attributedBody) > 0) "
        "     OR (m.balloon_bundle_id IS NOT NULL AND LENGTH(m.balloon_bundle_id) > 0)) "
        "ORDER BY m.ROWID ASC LIMIT 10";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    HU_ASSERT_EQ(rc, SQLITE_OK);
    sqlite3_bind_int64(stmt, 1, 0);

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
        count++;
    /* MSG-001 (text), MSG-004 (attachment placeholder won't match — no text/attributedBody/balloon),
     * MSG-005 (balloon_bundle_id) */
    HU_ASSERT_EQ(count, 2);

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

static void test_chatdb_tapback_query_counts_reactions(void) {
    sqlite3 *db = open_fixture();
    HU_ASSERT_NOT_NULL(db);

    const char *sql =
        "SELECT m.associated_message_type, COUNT(*) "
        "FROM message m "
        "WHERE m.is_from_me = 0 "
        "  AND m.associated_message_type BETWEEN 2000 AND 2006 "
        "  AND m.associated_message_guid IN ("
        "    SELECT m2.guid FROM message m2 "
        "    WHERE m2.is_from_me = 1 "
        "    AND m2.handle_id = (SELECT ROWID FROM handle WHERE id = ?1) "
        "    ORDER BY m2.date DESC LIMIT 5"
        "  ) "
        "GROUP BY m.associated_message_type";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    HU_ASSERT_EQ(rc, SQLITE_OK);
    sqlite3_bind_text(stmt, 1, "+15559999999", -1, NULL);

    int total = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int type = sqlite3_column_int(stmt, 0);
        int cnt = sqlite3_column_int(stmt, 1);
        HU_ASSERT_EQ(type, 2000);
        total += cnt;
    }
    HU_ASSERT_EQ(total, 1);

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

static void test_chatdb_history_query_returns_both_directions(void) {
    sqlite3 *db = open_fixture();
    HU_ASSERT_NOT_NULL(db);

    const char *sql =
        "SELECT m.is_from_me, m.text, m.attributedBody "
        "FROM message m "
        "JOIN handle h ON m.handle_id = h.ROWID "
        "WHERE h.id = ?1 AND m.associated_message_type = 0 "
        "ORDER BY m.date DESC LIMIT 10";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    HU_ASSERT_EQ(rc, SQLITE_OK);
    sqlite3_bind_text(stmt, 1, "+15559999999", -1, NULL);

    int inbound = 0, outbound = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (sqlite3_column_int(stmt, 0) == 0)
            inbound++;
        else
            outbound++;
    }
    HU_ASSERT_TRUE(inbound > 0);
    HU_ASSERT_TRUE(outbound > 0);

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

static void test_chatdb_attachment_join_works(void) {
    sqlite3 *db = open_fixture();
    HU_ASSERT_NOT_NULL(db);

    const char *sql =
        "SELECT a.filename FROM attachment a "
        "JOIN message_attachment_join maj ON maj.attachment_id = a.ROWID "
        "WHERE maj.message_id = ?1 LIMIT 1";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    HU_ASSERT_EQ(rc, SQLITE_OK);
    sqlite3_bind_int64(stmt, 1, 4);

    HU_ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    const char *fname = (const char *)sqlite3_column_text(stmt, 0);
    HU_ASSERT_NOT_NULL(fname);
    HU_ASSERT_TRUE(strstr(fname, "photo.jpg") != NULL);

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

static void test_chatdb_chat_guid_lookup(void) {
    sqlite3 *db = open_fixture();
    HU_ASSERT_NOT_NULL(db);

    const char *sql =
        "SELECT c.guid FROM chat c "
        "JOIN chat_message_join cmj ON c.ROWID = cmj.chat_id "
        "WHERE cmj.message_id = ? LIMIT 1";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    HU_ASSERT_EQ(rc, SQLITE_OK);
    sqlite3_bind_int64(stmt, 1, 1);

    HU_ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    const char *chat_guid = (const char *)sqlite3_column_text(stmt, 0);
    HU_ASSERT_STR_EQ(chat_guid, "iMessage;-;+15559999999");

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

static void test_chatdb_max_rowid(void) {
    sqlite3 *db = open_fixture();
    HU_ASSERT_NOT_NULL(db);

    const char *sql = "SELECT MAX(ROWID) FROM message";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    HU_ASSERT_EQ(rc, SQLITE_OK);
    HU_ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    int64_t max_rowid = sqlite3_column_int64(stmt, 0);
    HU_ASSERT_TRUE(max_rowid >= 5);

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

static void test_chatdb_participant_count_subquery(void) {
    sqlite3 *db = open_fixture();
    HU_ASSERT_NOT_NULL(db);

    const char *sql =
        "SELECT COALESCE("
        "  (SELECT COUNT(DISTINCT chj2.handle_id) FROM chat_message_join cmj "
        "   JOIN chat_handle_join chj2 ON chj2.chat_id = cmj.chat_id "
        "   WHERE cmj.message_id = ?), 0)";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    HU_ASSERT_EQ(rc, SQLITE_OK);
    sqlite3_bind_int64(stmt, 1, 1);
    HU_ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    int participants = sqlite3_column_int(stmt, 0);
    HU_ASSERT_EQ(participants, 1);

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

static void test_chatdb_balloon_bundle_id_detection(void) {
    sqlite3 *db = open_fixture();
    HU_ASSERT_NOT_NULL(db);

    const char *sql =
        "SELECT m.balloon_bundle_id FROM message m "
        "WHERE m.ROWID = 5 AND m.balloon_bundle_id IS NOT NULL";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    HU_ASSERT_EQ(rc, SQLITE_OK);
    HU_ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    const char *bundle = (const char *)sqlite3_column_text(stmt, 0);
    HU_ASSERT_NOT_NULL(bundle);
    HU_ASSERT_TRUE(strstr(bundle, "Sticker") != NULL);

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

void run_imessage_chatdb_fixture_tests(void) {
    HU_TEST_SUITE("iMessage ChatDB Fixture");
    HU_RUN_TEST(test_chatdb_schema_creates_successfully);
    HU_RUN_TEST(test_chatdb_poll_query_returns_inbound);
    HU_RUN_TEST(test_chatdb_tapback_query_counts_reactions);
    HU_RUN_TEST(test_chatdb_history_query_returns_both_directions);
    HU_RUN_TEST(test_chatdb_attachment_join_works);
    HU_RUN_TEST(test_chatdb_chat_guid_lookup);
    HU_RUN_TEST(test_chatdb_max_rowid);
    HU_RUN_TEST(test_chatdb_participant_count_subquery);
    HU_RUN_TEST(test_chatdb_balloon_bundle_id_detection);
}
#else
void run_imessage_chatdb_fixture_tests(void) {
    (void)0;
}
#endif
