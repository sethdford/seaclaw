#include "human/core/allocator.h"
#include "human/feeds/processor.h"
#include "human/core/string.h"
#include "test_framework.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#ifdef HU_ENABLE_SQLITE
#include "human/feeds/findings.h"
#include "human/feeds/trends.h"
#include <sqlite3.h>
#include <time.h>
#endif

static void feeds_create_table_sql_valid(void) {
    char buf[1024];
    size_t len = 0;
    hu_error_t err = hu_feeds_create_table_sql(buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_NOT_NULL(strstr(buf, "CREATE TABLE"));
    HU_ASSERT_NOT_NULL(strstr(buf, "feed_items"));
    HU_ASSERT_NOT_NULL(strstr(buf, "content_type"));
    HU_ASSERT_NOT_NULL(strstr(buf, "source"));
    HU_ASSERT_NOT_NULL(strstr(buf, "content"));
    HU_ASSERT_NOT_NULL(strstr(buf, "ingested_at"));
    HU_ASSERT_NOT_NULL(strstr(buf, "referenced"));
    HU_ASSERT_NOT_NULL(strstr(buf, "cluster_id"));
}

static void feeds_insert_sql_valid(void) {
    hu_feed_item_t item = {
        .id = 0,
        .type = HU_FEED_SOCIAL_INSTAGRAM,
        .source = "instagram:@friend",
        .source_len = 17,
        .content = "posted about hiking",
        .content_len = 19,
        .topic = "outdoors",
        .topic_len = 8,
        .relevance = 0.85,
        .fetched_at = 1700000000000ULL,
        .processed = false,
    };
    char buf[1024];
    size_t len = 0;
    hu_error_t err = hu_feeds_insert_sql(&item, buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_NOT_NULL(strstr(buf, "INSERT INTO"));
    HU_ASSERT_NOT_NULL(strstr(buf, "social_instagram"));
    HU_ASSERT_NOT_NULL(strstr(buf, "instagram:@friend"));
    HU_ASSERT_NOT_NULL(strstr(buf, "posted about hiking"));
}

static void feeds_query_unprocessed_sql_valid(void) {
    char buf[512];
    size_t len = 0;
    hu_error_t err =
        hu_feeds_query_unprocessed_sql(HU_FEED_NEWS_RSS, 10, buf,
                                        sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_NOT_NULL(strstr(buf, "SELECT"));
    HU_ASSERT_NOT_NULL(strstr(buf, "news_rss"));
    HU_ASSERT_NOT_NULL(strstr(buf, "ingested_at"));
    HU_ASSERT_NOT_NULL(strstr(buf, "LIMIT 10"));
}

static void feeds_mark_processed_sql_valid(void) {
    char buf[256];
    size_t len = 0;
    hu_error_t err = hu_feeds_mark_processed_sql(42, buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_NOT_NULL(strstr(buf, "UPDATE"));
    HU_ASSERT_NOT_NULL(strstr(buf, "referenced = 1"));
    HU_ASSERT_NOT_NULL(strstr(buf, "id = 42"));
}

static void feeds_query_by_topic_sql_valid(void) {
    char buf[512];
    size_t len = 0;
    const char *topic = "hiking";
    hu_error_t err = hu_feeds_query_by_topic_sql(
        topic, 6, 5, buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_NOT_NULL(strstr(buf, "SELECT"));
    HU_ASSERT_NOT_NULL(strstr(buf, "content LIKE"));
    HU_ASSERT_NOT_NULL(strstr(buf, "hiking"));
    HU_ASSERT_NOT_NULL(strstr(buf, "LIMIT 5"));
}

static void feeds_should_poll_enabled_interval_elapsed(void) {
    hu_feed_config_t config = {0};
    config.enabled[HU_FEED_MUSIC] = true;
    config.poll_interval_minutes[HU_FEED_MUSIC] = 60;
    uint64_t last = 1000000;
    uint64_t now = 1000000 + (60ULL * 60 * 1000);
    HU_ASSERT_TRUE(
        hu_feeds_should_poll(HU_FEED_MUSIC, &config, last, now));
}

static void feeds_should_poll_disabled_returns_false(void) {
    hu_feed_config_t config = {0};
    config.enabled[HU_FEED_EMAIL] = false;
    config.poll_interval_minutes[HU_FEED_EMAIL] = 30;
    uint64_t last = 1000000;
    uint64_t now = 1000000 + (60ULL * 60 * 1000);
    HU_ASSERT_FALSE(
        hu_feeds_should_poll(HU_FEED_EMAIL, &config, last, now));
}

static void feeds_should_poll_interval_not_elapsed_returns_false(void) {
    hu_feed_config_t config = {0};
    config.enabled[HU_FEED_NEWS_RSS] = true;
    config.poll_interval_minutes[HU_FEED_NEWS_RSS] = 60;
    uint64_t last = 1000000;
    uint64_t now = 1000000 + (30ULL * 60 * 1000);
    HU_ASSERT_FALSE(
        hu_feeds_should_poll(HU_FEED_NEWS_RSS, &config, last, now));
}

static void feeds_should_poll_interval_zero_returns_true_when_enabled(void) {
    hu_feed_config_t config = {0};
    config.enabled[HU_FEED_APPLE_REMINDERS] = true;
    config.poll_interval_minutes[HU_FEED_APPLE_REMINDERS] = 0;
    uint64_t last = 1000000;
    uint64_t now = 1000000;
    HU_ASSERT_TRUE(hu_feeds_should_poll(HU_FEED_APPLE_REMINDERS, &config,
                                         last, now));
}

static void feeds_feed_type_str_all_types(void) {
    HU_ASSERT_NOT_NULL(hu_feed_type_str(HU_FEED_SOCIAL_FACEBOOK));
    HU_ASSERT_STR_EQ(hu_feed_type_str(HU_FEED_SOCIAL_FACEBOOK),
                     "social_facebook");
    HU_ASSERT_STR_EQ(hu_feed_type_str(HU_FEED_SOCIAL_INSTAGRAM),
                     "social_instagram");
    HU_ASSERT_STR_EQ(hu_feed_type_str(HU_FEED_MUSIC), "music");
    HU_ASSERT_STR_EQ(hu_feed_type_str(HU_FEED_NEWS_RSS), "news_rss");
}

static void feeds_score_relevance_high_overlap(void) {
    size_t content_len = strlen("hiking mountains trail");
    size_t interest_len = strlen("hiking trail");
    double score = hu_feeds_score_relevance("hiking mountains trail",
                                          content_len, "hiking trail",
                                          interest_len);
    HU_ASSERT_TRUE(score >= 0.99);
}

static void feeds_score_relevance_no_overlap(void) {
    size_t content_len = strlen("cooking pizza");
    size_t interest_len = strlen("hiking trail");
    double score = hu_feeds_score_relevance("cooking pizza", content_len,
                                           "hiking trail", interest_len);
    HU_ASSERT_FLOAT_EQ(score, 0.0, 0.001);
}

static void feeds_score_relevance_partial(void) {
    size_t content_len = strlen("friend posted about hiking");
    size_t interest_len = strlen("hiking outdoors");
    double score = hu_feeds_score_relevance("friend posted about hiking",
                                           content_len, "hiking outdoors",
                                           interest_len);
    HU_ASSERT_TRUE(score > 0.0 && score < 1.0);
}

static void feeds_score_relevance_null_returns_zero(void) {
    double score = hu_feeds_score_relevance(NULL, 0, "hiking", 5);
    HU_ASSERT_FLOAT_EQ(score, 0.0, 0.001);
}

static void feeds_build_prompt_empty_returns_none(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_feeds_build_prompt(&alloc, NULL, 0, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_NOT_NULL(strstr(out, "[EXTERNAL AWARENESS]"));
    HU_ASSERT_NOT_NULL(strstr(out, "(none)"));
    hu_str_free(&alloc, out);
}

static void feeds_build_prompt_single_item(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_feed_item_t item = {
        .type = HU_FEED_SOCIAL_INSTAGRAM,
        .content = "friend posted about hiking",
        .content_len = 26,
    };
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_feeds_build_prompt(&alloc, &item, 1, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_NOT_NULL(strstr(out, "[EXTERNAL AWARENESS]"));
    HU_ASSERT_NOT_NULL(strstr(out, "Social"));
    HU_ASSERT_NOT_NULL(strstr(out, "friend posted about hiking"));
    hu_str_free(&alloc, out);
}

static void feeds_build_prompt_multiple_items(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_feed_item_t items[3] = {
        {
            .type = HU_FEED_SOCIAL_INSTAGRAM,
            .content = "friend posted about hiking",
            .content_len = 26,
        },
        {
            .type = HU_FEED_NEWS_RSS,
            .content = "new AI chip announced",
            .content_len = 21,
        },
        {
            .type = HU_FEED_MUSIC,
            .content = "listening to artist",
            .content_len = 19,
        },
    };
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err =
        hu_feeds_build_prompt(&alloc, items, 3, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_NOT_NULL(strstr(out, "[EXTERNAL AWARENESS]"));
    HU_ASSERT_NOT_NULL(strstr(out, "Social"));
    HU_ASSERT_NOT_NULL(strstr(out, "News"));
    HU_ASSERT_NOT_NULL(strstr(out, "Music"));
    HU_ASSERT_NOT_NULL(strstr(out, "friend posted about hiking"));
    HU_ASSERT_NOT_NULL(strstr(out, "new AI chip announced"));
    HU_ASSERT_NOT_NULL(strstr(out, "listening to artist"));
    hu_str_free(&alloc, out);
}

static void feeds_feed_item_deinit_frees_memory(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_feed_item_t item;
    memset(&item, 0, sizeof(item));
    item.source = hu_strndup(&alloc, "instagram:@friend", 17);
    item.source_len = 17;
    item.content = hu_strndup(&alloc, "test content", 11);
    item.content_len = 11;
    item.topic = hu_strndup(&alloc, "outdoors", 8);
    item.topic_len = 8;
    HU_ASSERT_NOT_NULL(item.source);
    HU_ASSERT_NOT_NULL(item.content);
    HU_ASSERT_NOT_NULL(item.topic);
    hu_feed_item_deinit(&alloc, &item);
    HU_ASSERT_NULL(item.source);
    HU_ASSERT_NULL(item.content);
    HU_ASSERT_NULL(item.topic);
    HU_ASSERT_EQ(item.source_len, 0);
    HU_ASSERT_EQ(item.content_len, 0);
    HU_ASSERT_EQ(item.topic_len, 0);
}

#ifdef HU_ENABLE_SQLITE

static void feed_search_fts_match(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);

    const char *schema =
        "CREATE TABLE IF NOT EXISTS feed_items("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "source TEXT NOT NULL,"
        "contact_id TEXT,"
        "content_type TEXT NOT NULL,"
        "content TEXT NOT NULL,"
        "url TEXT,"
        "ingested_at INTEGER NOT NULL,"
        "referenced INTEGER DEFAULT 0,"
        "cluster_id INTEGER DEFAULT NULL)";
    HU_ASSERT_EQ(sqlite3_exec(db, schema, NULL, NULL, NULL), SQLITE_OK);

    const char *fts =
        "CREATE VIRTUAL TABLE IF NOT EXISTS feed_items_fts USING fts5("
        "content, source, content_type, content=feed_items, content_rowid=id)";
    HU_ASSERT_EQ(sqlite3_exec(db, fts, NULL, NULL, NULL), SQLITE_OK);

    const char *trig_ai =
        "CREATE TRIGGER IF NOT EXISTS feed_items_ai AFTER INSERT ON feed_items "
        "BEGIN INSERT INTO feed_items_fts(rowid, content, source, content_type) "
        "VALUES (new.id, new.content, new.source, new.content_type); END";
    HU_ASSERT_EQ(sqlite3_exec(db, trig_ai, NULL, NULL, NULL), SQLITE_OK);

    const char *trig_ad =
        "CREATE TRIGGER IF NOT EXISTS feed_items_ad AFTER DELETE ON feed_items "
        "BEGIN INSERT INTO feed_items_fts(feed_items_fts, rowid, content, source, content_type) "
        "VALUES ('delete', old.id, old.content, old.source, old.content_type); END";
    HU_ASSERT_EQ(sqlite3_exec(db, trig_ad, NULL, NULL, NULL), SQLITE_OK);

    int64_t now = (int64_t)time(NULL);
    sqlite3_stmt *ins = NULL;
    const char *ins_sql =
        "INSERT INTO feed_items(source, content_type, content, ingested_at) VALUES (?,?,?,?)";
    HU_ASSERT_EQ(sqlite3_prepare_v2(db, ins_sql, -1, &ins, NULL), SQLITE_OK);

    const char *src = "rss";
    const char *ct = "article";
    sqlite3_bind_text(ins, 1, src, -1, SQLITE_STATIC);
    sqlite3_bind_text(ins, 2, ct, -1, SQLITE_STATIC);
    sqlite3_bind_text(ins, 3, "Advances in machine learning", -1, SQLITE_STATIC);
    sqlite3_bind_int64(ins, 4, now);
    HU_ASSERT_EQ(sqlite3_step(ins), SQLITE_DONE);
    sqlite3_reset(ins);

    sqlite3_bind_text(ins, 1, src, -1, SQLITE_STATIC);
    sqlite3_bind_text(ins, 2, ct, -1, SQLITE_STATIC);
    sqlite3_bind_text(ins, 3, "Deep learning and machine learning", -1, SQLITE_STATIC);
    sqlite3_bind_int64(ins, 4, now);
    HU_ASSERT_EQ(sqlite3_step(ins), SQLITE_DONE);
    sqlite3_reset(ins);

    sqlite3_bind_text(ins, 1, "twitter", -1, SQLITE_STATIC);
    sqlite3_bind_text(ins, 2, "tweet", -1, SQLITE_STATIC);
    sqlite3_bind_text(ins, 3, "Cooking pasta recipes", -1, SQLITE_STATIC);
    sqlite3_bind_int64(ins, 4, now);
    HU_ASSERT_EQ(sqlite3_step(ins), SQLITE_DONE);
    sqlite3_finalize(ins);

    hu_feed_item_stored_t *out = NULL;
    size_t count = 0;
    hu_error_t err = hu_feed_search(&alloc, db, "machine learning", 16, 10, &out, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, 2u);
    hu_feed_items_free(&alloc, out, count);
    sqlite3_close(db);
}

static void feed_build_daily_digest_groups_by_source(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);

    const char *schema =
        "CREATE TABLE IF NOT EXISTS feed_items("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "source TEXT NOT NULL,"
        "contact_id TEXT,"
        "content_type TEXT NOT NULL,"
        "content TEXT NOT NULL,"
        "url TEXT,"
        "ingested_at INTEGER NOT NULL,"
        "referenced INTEGER DEFAULT 0,"
        "cluster_id INTEGER DEFAULT NULL)";
    HU_ASSERT_EQ(sqlite3_exec(db, schema, NULL, NULL, NULL), SQLITE_OK);

    int64_t now = (int64_t)time(NULL);
    sqlite3_stmt *ins = NULL;
    const char *ins_sql =
        "INSERT INTO feed_items(source, content_type, content, ingested_at) VALUES (?,?,?,?)";
    HU_ASSERT_EQ(sqlite3_prepare_v2(db, ins_sql, -1, &ins, NULL), SQLITE_OK);

    const char *sources[2] = {"rss", "twitter"};
    for (int i = 0; i < 5; i++) {
        sqlite3_bind_text(ins, 1, sources[i % 2], -1, SQLITE_STATIC);
        sqlite3_bind_text(ins, 2, "post", -1, SQLITE_STATIC);
        sqlite3_bind_text(ins, 3, "Item content", -1, SQLITE_STATIC);
        sqlite3_bind_int64(ins, 4, now);
        HU_ASSERT_EQ(sqlite3_step(ins), SQLITE_DONE);
        sqlite3_reset(ins);
    }
    sqlite3_finalize(ins);

    char *out = NULL;
    size_t out_len = 0;
    int64_t since_ts = now - 86400;
    hu_error_t err = hu_feed_build_daily_digest(&alloc, db, since_ts, 4000, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_NOT_NULL(strstr(out, "rss"));
    HU_ASSERT_NOT_NULL(strstr(out, "twitter"));
    hu_str_free(&alloc, out);
    sqlite3_close(db);
}

static void feed_processor_cleanup_removes_old(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);

    const char *schema =
        "CREATE TABLE IF NOT EXISTS feed_items("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "source TEXT NOT NULL,"
        "contact_id TEXT,"
        "content_type TEXT NOT NULL,"
        "content TEXT NOT NULL,"
        "url TEXT,"
        "ingested_at INTEGER NOT NULL,"
        "referenced INTEGER DEFAULT 0,"
        "cluster_id INTEGER DEFAULT NULL)";
    HU_ASSERT_EQ(sqlite3_exec(db, schema, NULL, NULL, NULL), SQLITE_OK);

    int64_t now = (int64_t)time(NULL);
    int64_t old_ts = now - (60 * 86400);

    sqlite3_stmt *ins = NULL;
    const char *ins_sql =
        "INSERT INTO feed_items(source, content_type, content, ingested_at) VALUES (?,?,?,?)";
    HU_ASSERT_EQ(sqlite3_prepare_v2(db, ins_sql, -1, &ins, NULL), SQLITE_OK);

    sqlite3_bind_text(ins, 1, "rss", -1, SQLITE_STATIC);
    sqlite3_bind_text(ins, 2, "article", -1, SQLITE_STATIC);
    sqlite3_bind_text(ins, 3, "Recent item", -1, SQLITE_STATIC);
    sqlite3_bind_int64(ins, 4, now);
    HU_ASSERT_EQ(sqlite3_step(ins), SQLITE_DONE);
    sqlite3_reset(ins);

    sqlite3_bind_text(ins, 1, "twitter", -1, SQLITE_STATIC);
    sqlite3_bind_text(ins, 2, "tweet", -1, SQLITE_STATIC);
    sqlite3_bind_text(ins, 3, "Old item", -1, SQLITE_STATIC);
    sqlite3_bind_int64(ins, 4, old_ts);
    HU_ASSERT_EQ(sqlite3_step(ins), SQLITE_DONE);
    sqlite3_finalize(ins);

    hu_feed_processor_t proc = {.alloc = &alloc, .db = db};
    hu_error_t err = hu_feed_processor_cleanup(&proc, 30);
    HU_ASSERT_EQ(err, HU_OK);

    sqlite3_stmt *sel = NULL;
    HU_ASSERT_EQ(sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM feed_items", -1, &sel, NULL), SQLITE_OK);
    HU_ASSERT_EQ(sqlite3_step(sel), SQLITE_ROW);
    HU_ASSERT_EQ(sqlite3_column_int(sel, 0), 1);
    sqlite3_finalize(sel);
    sqlite3_close(db);
}

static void feed_get_all_recent_returns_items(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);

    const char *schema =
        "CREATE TABLE IF NOT EXISTS feed_items("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "source TEXT NOT NULL,"
        "contact_id TEXT,"
        "content_type TEXT NOT NULL,"
        "content TEXT NOT NULL,"
        "url TEXT,"
        "ingested_at INTEGER NOT NULL,"
        "referenced INTEGER DEFAULT 0,"
        "cluster_id INTEGER DEFAULT NULL)";
    HU_ASSERT_EQ(sqlite3_exec(db, schema, NULL, NULL, NULL), SQLITE_OK);

    int64_t now = (int64_t)time(NULL);
    int64_t since = now - 86400;
    int64_t old_ts = now - (2 * 86400);

    sqlite3_stmt *ins = NULL;
    const char *ins_sql =
        "INSERT INTO feed_items(source, content_type, content, ingested_at) VALUES (?,?,?,?)";
    HU_ASSERT_EQ(sqlite3_prepare_v2(db, ins_sql, -1, &ins, NULL), SQLITE_OK);

    sqlite3_bind_text(ins, 1, "rss", -1, SQLITE_STATIC);
    sqlite3_bind_text(ins, 2, "article", -1, SQLITE_STATIC);
    sqlite3_bind_text(ins, 3, "Recent 1", -1, SQLITE_STATIC);
    sqlite3_bind_int64(ins, 4, now);
    HU_ASSERT_EQ(sqlite3_step(ins), SQLITE_DONE);
    sqlite3_reset(ins);

    sqlite3_bind_text(ins, 1, "twitter", -1, SQLITE_STATIC);
    sqlite3_bind_text(ins, 2, "tweet", -1, SQLITE_STATIC);
    sqlite3_bind_text(ins, 3, "Recent 2", -1, SQLITE_STATIC);
    sqlite3_bind_int64(ins, 4, now - 3600);
    HU_ASSERT_EQ(sqlite3_step(ins), SQLITE_DONE);
    sqlite3_reset(ins);

    sqlite3_bind_text(ins, 1, "rss", -1, SQLITE_STATIC);
    sqlite3_bind_text(ins, 2, "article", -1, SQLITE_STATIC);
    sqlite3_bind_text(ins, 3, "Old item", -1, SQLITE_STATIC);
    sqlite3_bind_int64(ins, 4, old_ts);
    HU_ASSERT_EQ(sqlite3_step(ins), SQLITE_DONE);
    sqlite3_finalize(ins);

    hu_feed_item_stored_t *out = NULL;
    size_t count = 0;
    hu_error_t err = hu_feed_processor_get_all_recent(&alloc, db, since, 10, &out, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, 2u);
    hu_feed_items_free(&alloc, out, count);
    sqlite3_close(db);
}

static void feed_correlate_groups_similar(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);

    const char *schema =
        "CREATE TABLE IF NOT EXISTS feed_items("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "source TEXT NOT NULL,"
        "contact_id TEXT,"
        "content_type TEXT NOT NULL,"
        "content TEXT NOT NULL,"
        "url TEXT,"
        "ingested_at INTEGER NOT NULL,"
        "referenced INTEGER DEFAULT 0,"
        "cluster_id INTEGER DEFAULT NULL)";
    HU_ASSERT_EQ(sqlite3_exec(db, schema, NULL, NULL, NULL), SQLITE_OK);

    int64_t now = (int64_t)time(NULL);
    int64_t since = now - 86400;

    sqlite3_stmt *ins = NULL;
    const char *ins_sql =
        "INSERT INTO feed_items(source, content_type, content, ingested_at) VALUES (?,?,?,?)";
    HU_ASSERT_EQ(sqlite3_prepare_v2(db, ins_sql, -1, &ins, NULL), SQLITE_OK);

    sqlite3_bind_text(ins, 1, "rss", -1, SQLITE_STATIC);
    sqlite3_bind_text(ins, 2, "article", -1, SQLITE_STATIC);
    sqlite3_bind_text(ins, 3, "AI agent developments in tech", -1, SQLITE_STATIC);
    sqlite3_bind_int64(ins, 4, now);
    HU_ASSERT_EQ(sqlite3_step(ins), SQLITE_DONE);
    sqlite3_reset(ins);

    sqlite3_bind_text(ins, 1, "twitter", -1, SQLITE_STATIC);
    sqlite3_bind_text(ins, 2, "tweet", -1, SQLITE_STATIC);
    sqlite3_bind_text(ins, 3, "AI agent news and updates", -1, SQLITE_STATIC);
    sqlite3_bind_int64(ins, 4, now);
    HU_ASSERT_EQ(sqlite3_step(ins), SQLITE_DONE);
    sqlite3_reset(ins);

    sqlite3_bind_text(ins, 1, "instagram", -1, SQLITE_STATIC);
    sqlite3_bind_text(ins, 2, "post", -1, SQLITE_STATIC);
    sqlite3_bind_text(ins, 3, "Cooking recipes for dinner", -1, SQLITE_STATIC);
    sqlite3_bind_int64(ins, 4, now);
    HU_ASSERT_EQ(sqlite3_step(ins), SQLITE_DONE);
    sqlite3_finalize(ins);

    hu_error_t err = hu_feed_correlate_recent(&alloc, db, since, 0.3);
    HU_ASSERT_EQ(err, HU_OK);

    sqlite3_stmt *sel = NULL;
    HU_ASSERT_EQ(sqlite3_prepare_v2(db, "SELECT cluster_id FROM feed_items WHERE cluster_id IS NOT NULL", -1, &sel, NULL), SQLITE_OK);
    int has_cluster = 0;
    while (sqlite3_step(sel) == SQLITE_ROW) {
        has_cluster = 1;
        break;
    }
    sqlite3_finalize(sel);
    HU_ASSERT_TRUE(has_cluster);
    sqlite3_close(db);
}

static void findings_store_and_retrieve(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);

    const char *schema =
        "CREATE TABLE IF NOT EXISTS research_findings("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "source TEXT,"
        "finding TEXT NOT NULL,"
        "relevance TEXT,"
        "priority TEXT DEFAULT 'MEDIUM',"
        "suggested_action TEXT,"
        "status TEXT DEFAULT 'pending',"
        "created_at INTEGER NOT NULL,"
        "acted_at INTEGER)";
    HU_ASSERT_EQ(sqlite3_exec(db, schema, NULL, NULL, NULL), SQLITE_OK);

    hu_error_t err = hu_findings_store(&alloc, db, "test", "A finding", "HIGH", "HIGH", "Do something");
    HU_ASSERT_EQ(err, HU_OK);

    hu_research_finding_t *items = NULL;
    size_t count = 0;
    err = hu_findings_get_pending(&alloc, db, 10, &items, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, 1u);
    HU_ASSERT_NOT_NULL(strstr(items[0].finding, "A finding"));
    hu_findings_free(&alloc, items, count);
    sqlite3_close(db);
}

static void findings_mark_status(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);

    const char *schema =
        "CREATE TABLE IF NOT EXISTS research_findings("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "source TEXT,"
        "finding TEXT NOT NULL,"
        "relevance TEXT,"
        "priority TEXT DEFAULT 'MEDIUM',"
        "suggested_action TEXT,"
        "status TEXT DEFAULT 'pending',"
        "created_at INTEGER NOT NULL,"
        "acted_at INTEGER)";
    HU_ASSERT_EQ(sqlite3_exec(db, schema, NULL, NULL, NULL), SQLITE_OK);

    hu_error_t err = hu_findings_store(&alloc, db, "test", "A finding", "HIGH", "HIGH", "Do something");
    HU_ASSERT_EQ(err, HU_OK);

    err = hu_findings_mark_status(db, 1, "completed");
    HU_ASSERT_EQ(err, HU_OK);

    hu_research_finding_t *items = NULL;
    size_t count = 0;
    err = hu_findings_get_pending(&alloc, db, 10, &items, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, 0u);
    sqlite3_close(db);
}

static void feed_detect_trends_no_spikes(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);

    const char *schema =
        "CREATE TABLE IF NOT EXISTS feed_items("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "source TEXT NOT NULL,"
        "contact_id TEXT,"
        "content_type TEXT NOT NULL,"
        "content TEXT NOT NULL,"
        "url TEXT,"
        "ingested_at INTEGER NOT NULL,"
        "referenced INTEGER DEFAULT 0,"
        "cluster_id INTEGER DEFAULT NULL)";
    HU_ASSERT_EQ(sqlite3_exec(db, schema, NULL, NULL, NULL), SQLITE_OK);

    int64_t now = (int64_t)time(NULL);
    sqlite3_stmt *ins = NULL;
    const char *ins_sql =
        "INSERT INTO feed_items(source, content_type, content, ingested_at) VALUES (?,?,?,?)";
    HU_ASSERT_EQ(sqlite3_prepare_v2(db, ins_sql, -1, &ins, NULL), SQLITE_OK);
    sqlite3_bind_text(ins, 1, "rss", -1, SQLITE_STATIC);
    sqlite3_bind_text(ins, 2, "article", -1, SQLITE_STATIC);
    sqlite3_bind_text(ins, 3, "Single item", -1, SQLITE_STATIC);
    sqlite3_bind_int64(ins, 4, now);
    HU_ASSERT_EQ(sqlite3_step(ins), SQLITE_DONE);
    sqlite3_finalize(ins);

    hu_feed_trend_t *trends = NULL;
    size_t count = 0;
    const char *keywords = "AI blockchain";
    hu_error_t err = hu_feed_detect_trends(&alloc, db, keywords, strlen(keywords), &trends, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, 0u);
    hu_feed_trends_free(&alloc, trends, count);
    sqlite3_close(db);
}

static void feed_trends_build_section_empty(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t len = 0;
    hu_error_t err = hu_feed_trends_build_section(&alloc, NULL, 0, &out, &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_NOT_NULL(strstr(out, "No significant trends"));
    hu_str_free(&alloc, out);
}

#endif /* HU_ENABLE_SQLITE */

void run_feeds_tests(void) {
    HU_TEST_SUITE("feeds");
    HU_RUN_TEST(feeds_create_table_sql_valid);
    HU_RUN_TEST(feeds_insert_sql_valid);
    HU_RUN_TEST(feeds_query_unprocessed_sql_valid);
    HU_RUN_TEST(feeds_mark_processed_sql_valid);
    HU_RUN_TEST(feeds_query_by_topic_sql_valid);
    HU_RUN_TEST(feeds_should_poll_enabled_interval_elapsed);
    HU_RUN_TEST(feeds_should_poll_disabled_returns_false);
    HU_RUN_TEST(feeds_should_poll_interval_not_elapsed_returns_false);
    HU_RUN_TEST(feeds_should_poll_interval_zero_returns_true_when_enabled);
    HU_RUN_TEST(feeds_feed_type_str_all_types);
    HU_RUN_TEST(feeds_score_relevance_high_overlap);
    HU_RUN_TEST(feeds_score_relevance_no_overlap);
    HU_RUN_TEST(feeds_score_relevance_partial);
    HU_RUN_TEST(feeds_score_relevance_null_returns_zero);
    HU_RUN_TEST(feeds_build_prompt_empty_returns_none);
    HU_RUN_TEST(feeds_build_prompt_single_item);
    HU_RUN_TEST(feeds_build_prompt_multiple_items);
    HU_RUN_TEST(feeds_feed_item_deinit_frees_memory);
#ifdef HU_ENABLE_SQLITE
    HU_RUN_TEST(feed_search_fts_match);
    HU_RUN_TEST(feed_build_daily_digest_groups_by_source);
    HU_RUN_TEST(feed_processor_cleanup_removes_old);
    HU_RUN_TEST(feed_get_all_recent_returns_items);
    HU_RUN_TEST(feed_correlate_groups_similar);
    HU_RUN_TEST(findings_store_and_retrieve);
    HU_RUN_TEST(findings_mark_status);
    HU_RUN_TEST(feed_detect_trends_no_spikes);
    HU_RUN_TEST(feed_trends_build_section_empty);
#endif
}
