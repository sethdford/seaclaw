#include "human/core/allocator.h"
#include "human/memory/verify_claim.h"
#include "test_framework.h"
#include <string.h>

/* --- hu_memory_has_claim_language tests --- */

static void claim_language_detects_i_remember(void) {
    const char *text = "I remember when you told me about your trip";
    HU_ASSERT_TRUE(hu_memory_has_claim_language(text, strlen(text)));
}

static void claim_language_detects_you_told_me(void) {
    const char *text = "you told me that you like coffee";
    HU_ASSERT_TRUE(hu_memory_has_claim_language(text, strlen(text)));
}

static void claim_language_detects_we_discussed(void) {
    const char *text = "we discussed this topic last week";
    HU_ASSERT_TRUE(hu_memory_has_claim_language(text, strlen(text)));
}

static void claim_language_case_insensitive(void) {
    const char *text = "YOU TOLD ME about your favorite food";
    HU_ASSERT_TRUE(hu_memory_has_claim_language(text, strlen(text)));
}

static void claim_language_no_match_returns_false(void) {
    const char *text = "The weather is nice today";
    HU_ASSERT_TRUE(!hu_memory_has_claim_language(text, strlen(text)));
}

static void claim_language_null_returns_false(void) {
    HU_ASSERT_TRUE(!hu_memory_has_claim_language(NULL, 0));
}

static void claim_language_empty_returns_false(void) {
    HU_ASSERT_TRUE(!hu_memory_has_claim_language("", 0));
}

static void claim_language_detects_last_time_we(void) {
    const char *text = "Last time we talked about this topic";
    HU_ASSERT_TRUE(hu_memory_has_claim_language(text, strlen(text)));
}

static void claim_language_detects_you_mentioned(void) {
    const char *text = "you mentioned something about dogs";
    HU_ASSERT_TRUE(hu_memory_has_claim_language(text, strlen(text)));
}

static void claim_language_detects_from_our_conversation(void) {
    const char *text = "from our conversation I gathered that";
    HU_ASSERT_TRUE(hu_memory_has_claim_language(text, strlen(text)));
}

/* --- hu_memory_hedge_claim tests --- */

static void hedge_replaces_i_remember(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *text = "I remember you like coffee";
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_memory_hedge_claim(&alloc, text, strlen(text), &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_STR_CONTAINS(out, "I think I remember");
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void hedge_replaces_you_told_me(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *text = "you told me about your cat";
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_memory_hedge_claim(&alloc, text, strlen(text), &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_STR_CONTAINS(out, "I believe you told me");
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void hedge_prepends_when_no_specific_pattern(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *text = "you mentioned your dog likes walks";
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_memory_hedge_claim(&alloc, text, strlen(text), &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    /* Should prepend "I think " since "you mentioned" doesn't have a specific replacement */
    HU_ASSERT_TRUE(out_len > strlen(text));
    HU_ASSERT_TRUE(memcmp(out, "I think ", 8) == 0);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void hedge_null_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_error_t err = hu_memory_hedge_claim(&alloc, NULL, 0, NULL, NULL);
    HU_ASSERT_NEQ(err, HU_OK);
}

/* --- hu_memory_verify_claim tests (SQLite-gated) --- */

#ifdef HU_ENABLE_SQLITE
#include "human/memory/episodic.h"
#include <sqlite3.h>

static void verify_claim_no_episodes_returns_zero_confidence(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    int rc = sqlite3_open(":memory:", &db);
    HU_ASSERT_EQ(rc, SQLITE_OK);

    /* Create episodes table */
    const char *sql =
        "CREATE TABLE IF NOT EXISTS episodes ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "contact_id TEXT NOT NULL DEFAULT '',"
        "summary TEXT NOT NULL DEFAULT '',"
        "emotional_arc TEXT NOT NULL DEFAULT '',"
        "key_moments TEXT NOT NULL DEFAULT '',"
        "impact_score REAL DEFAULT 0.0,"
        "salience_score REAL DEFAULT 0.0,"
        "last_reinforced_at INTEGER DEFAULT 0,"
        "source TEXT NOT NULL DEFAULT '',"
        "created_at INTEGER DEFAULT 0)";
    rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
    HU_ASSERT_EQ(rc, SQLITE_OK);

    hu_claim_result_t result;
    hu_error_t err = hu_memory_verify_claim(&alloc, db, "alice", 5,
                                             "you told me you like hiking", 27, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.confidence < 0.01);
    HU_ASSERT_TRUE(!result.has_provenance);

    sqlite3_close(db);
}

static void verify_claim_matching_episode_returns_confidence(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    int rc = sqlite3_open(":memory:", &db);
    HU_ASSERT_EQ(rc, SQLITE_OK);

    const char *sql =
        "CREATE TABLE IF NOT EXISTS episodes ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "contact_id TEXT NOT NULL DEFAULT '',"
        "summary TEXT NOT NULL DEFAULT '',"
        "emotional_arc TEXT NOT NULL DEFAULT '',"
        "key_moments TEXT NOT NULL DEFAULT '',"
        "impact_score REAL DEFAULT 0.0,"
        "salience_score REAL DEFAULT 0.0,"
        "last_reinforced_at INTEGER DEFAULT 0,"
        "source TEXT NOT NULL DEFAULT '',"
        "created_at INTEGER DEFAULT 0)";
    rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
    HU_ASSERT_EQ(rc, SQLITE_OK);

    /* Store an episode for alice about hiking */
    int64_t ep_id = 0;
    hu_error_t err = hu_episode_store_insert(
        &alloc, db, "alice", 5, "alice loves hiking in the mountains", 35,
        "positive", 8, "you love hiking in the mountains", 31, 0.8, "chat", 4, &ep_id);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(ep_id > 0);

    /* Use a claim that will LIKE-match against key_moments */
    hu_claim_result_t result;
    err = hu_memory_verify_claim(&alloc, db, "alice", 5,
                                  "love hiking", 11, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.confidence > 0.0);
    HU_ASSERT_TRUE(result.contact_match);
    HU_ASSERT_TRUE(result.timestamp_ok);

    sqlite3_close(db);
}

static void verify_claim_cross_contact_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    int rc = sqlite3_open(":memory:", &db);
    HU_ASSERT_EQ(rc, SQLITE_OK);

    const char *sql =
        "CREATE TABLE IF NOT EXISTS episodes ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "contact_id TEXT NOT NULL DEFAULT '',"
        "summary TEXT NOT NULL DEFAULT '',"
        "emotional_arc TEXT NOT NULL DEFAULT '',"
        "key_moments TEXT NOT NULL DEFAULT '',"
        "impact_score REAL DEFAULT 0.0,"
        "salience_score REAL DEFAULT 0.0,"
        "last_reinforced_at INTEGER DEFAULT 0,"
        "source TEXT NOT NULL DEFAULT '',"
        "created_at INTEGER DEFAULT 0)";
    rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
    HU_ASSERT_EQ(rc, SQLITE_OK);

    /* Store episode for alice */
    int64_t ep_id = 0;
    hu_error_t err = hu_episode_store_insert(
        &alloc, db, "alice", 5, "alice said she likes coffee every morning", 41,
        "neutral", 7, "coffee preference noted", 22, 0.5, "chat", 4, &ep_id);
    HU_ASSERT_EQ(err, HU_OK);

    /* Verify claim for bob — should have low confidence due to contact mismatch */
    hu_claim_result_t result;
    err = hu_memory_verify_claim(&alloc, db, "bob", 3,
                                  "you said you like coffee", 24, &result);
    HU_ASSERT_EQ(err, HU_OK);
    /* contact_match should be false since episode belongs to alice, not bob */
    HU_ASSERT_TRUE(!result.contact_match);
    /* confidence should be severely penalized */
    HU_ASSERT_TRUE(result.confidence < 0.1);

    sqlite3_close(db);
}

static void verify_claim_null_args_returns_error(void) {
    hu_error_t err = hu_memory_verify_claim(NULL, NULL, NULL, 0, NULL, 0, NULL);
    HU_ASSERT_NEQ(err, HU_OK);
}

#endif /* HU_ENABLE_SQLITE */

void run_verify_claim_tests(void) {
    HU_TEST_SUITE("verify_claim");
    HU_RUN_TEST(claim_language_detects_i_remember);
    HU_RUN_TEST(claim_language_detects_you_told_me);
    HU_RUN_TEST(claim_language_detects_we_discussed);
    HU_RUN_TEST(claim_language_case_insensitive);
    HU_RUN_TEST(claim_language_no_match_returns_false);
    HU_RUN_TEST(claim_language_null_returns_false);
    HU_RUN_TEST(claim_language_empty_returns_false);
    HU_RUN_TEST(claim_language_detects_last_time_we);
    HU_RUN_TEST(claim_language_detects_you_mentioned);
    HU_RUN_TEST(claim_language_detects_from_our_conversation);
    HU_RUN_TEST(hedge_replaces_i_remember);
    HU_RUN_TEST(hedge_replaces_you_told_me);
    HU_RUN_TEST(hedge_prepends_when_no_specific_pattern);
    HU_RUN_TEST(hedge_null_returns_error);
#ifdef HU_ENABLE_SQLITE
    HU_RUN_TEST(verify_claim_no_episodes_returns_zero_confidence);
    HU_RUN_TEST(verify_claim_matching_episode_returns_confidence);
    HU_RUN_TEST(verify_claim_cross_contact_fails);
    HU_RUN_TEST(verify_claim_null_args_returns_error);
#endif
}
