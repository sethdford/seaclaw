#include "test_framework.h"
#include <stdlib.h>
#include <string.h>

#ifdef HU_ENABLE_SQLITE

#include "human/memory/evolved_opinions.h"
#include <sqlite3.h>

static sqlite3 *s_db = NULL;

static void setup_db(void) {
    if (s_db) {
        sqlite3_close(s_db);
        s_db = NULL;
    }
    sqlite3_open(":memory:", &s_db);
    hu_evolved_opinions_ensure_table(s_db);
}

static void teardown_db(void) {
    if (s_db) {
        sqlite3_close(s_db);
        s_db = NULL;
    }
}

#define S(lit) (lit), (sizeof(lit) - 1)

/* ── hu_evolved_opinion_find tests ───────────────────────────────────── */

static void find_returns_existing_opinion(void) {
    setup_db();
    hu_allocator_t alloc = hu_system_allocator();
    hu_evolved_opinion_upsert(s_db, S("remote work"), S("generally positive"), 0.8, 1000);

    hu_evolved_opinion_t op;
    bool found = false;
    hu_error_t err = hu_evolved_opinion_find(&alloc, s_db, S("remote work"), &op, &found);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(found);
    HU_ASSERT_STR_EQ(op.topic, "remote work");
    HU_ASSERT_STR_EQ(op.stance, "generally positive");
    HU_ASSERT(op.conviction > 0.7);
    alloc.free(alloc.ctx, op.topic, op.topic_len + 1);
    alloc.free(alloc.ctx, op.stance, op.stance_len + 1);
    teardown_db();
}

static void find_returns_not_found(void) {
    setup_db();
    hu_allocator_t alloc = hu_system_allocator();

    hu_evolved_opinion_t op;
    bool found = false;
    hu_error_t err = hu_evolved_opinion_find(&alloc, s_db, S("nonexistent"), &op, &found);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_FALSE(found);
    teardown_db();
}

static void find_null_args_returns_error(void) {
    setup_db();
    hu_allocator_t alloc = hu_system_allocator();
    hu_evolved_opinion_t op;
    bool found = false;

    HU_ASSERT_EQ(hu_evolved_opinion_find(NULL, s_db, S("x"), &op, &found), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_evolved_opinion_find(&alloc, NULL, S("x"), &op, &found),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_evolved_opinion_find(&alloc, s_db, NULL, 0, &op, &found),
                 HU_ERR_INVALID_ARGUMENT);
    teardown_db();
}

/* ── hu_opinion_check_before_agree tests ─────────────────────────────── */

static void check_before_agree_strong_opinion_injects_directive(void) {
    setup_db();
    hu_allocator_t alloc = hu_system_allocator();
    hu_evolved_opinion_upsert(s_db, S("TDD"), S("essential for quality"), 0.9, 1000);

    size_t len = 0;
    char *directive = hu_opinion_check_before_agree(&alloc, s_db, S("TDD"), &len);
    HU_ASSERT_NOT_NULL(directive);
    HU_ASSERT(len > 0);
    HU_ASSERT(strstr(directive, "You have a view on TDD") != NULL);
    HU_ASSERT(strstr(directive, "Lead with your actual opinion") != NULL);
    alloc.free(alloc.ctx, directive, len + 1);
    teardown_db();
}

static void check_before_agree_weak_opinion_returns_null(void) {
    setup_db();
    hu_allocator_t alloc = hu_system_allocator();
    hu_evolved_opinion_upsert(s_db, S("tabs vs spaces"), S("no strong preference"), 0.3, 1000);

    size_t len = 0;
    char *directive = hu_opinion_check_before_agree(&alloc, s_db, S("tabs vs spaces"), &len);
    HU_ASSERT_NULL(directive);
    teardown_db();
}

static void check_before_agree_no_opinion_returns_null(void) {
    setup_db();
    hu_allocator_t alloc = hu_system_allocator();

    size_t len = 0;
    char *directive = hu_opinion_check_before_agree(&alloc, s_db, S("unknown topic"), &len);
    HU_ASSERT_NULL(directive);
    teardown_db();
}

static void check_before_agree_null_args_returns_null(void) {
    hu_allocator_t alloc = hu_system_allocator();
    HU_ASSERT_NULL(hu_opinion_check_before_agree(NULL, s_db, S("x"), NULL));
    HU_ASSERT_NULL(hu_opinion_check_before_agree(&alloc, NULL, S("x"), NULL));
    HU_ASSERT_NULL(hu_opinion_check_before_agree(&alloc, s_db, NULL, 0, NULL));
}

/* ── hu_opinion_contrarian_prompt tests ──────────────────────────────── */

static void contrarian_fires_approximately_15_percent(void) {
    hu_allocator_t alloc = hu_system_allocator();
    uint32_t fired = 0;
    uint32_t total = 1000;

    for (uint32_t i = 0; i < total; i++) {
        size_t len = 0;
        char *p = hu_opinion_contrarian_prompt(&alloc, S("test topic"), i, &len);
        if (p) {
            fired++;
            alloc.free(alloc.ctx, p, len + 1);
        }
    }

    /* Should be roughly 15% — allow 8%-25% range */
    HU_ASSERT(fired > 80);
    HU_ASSERT(fired < 250);
}

static void contrarian_null_args_returns_null(void) {
    hu_allocator_t alloc = hu_system_allocator();
    HU_ASSERT_NULL(hu_opinion_contrarian_prompt(NULL, S("x"), 0, NULL));
    HU_ASSERT_NULL(hu_opinion_contrarian_prompt(&alloc, NULL, 0, 0, NULL));
}

#endif /* HU_ENABLE_SQLITE */

void run_anti_sycophancy_tests(void) {
    HU_TEST_SUITE("anti_sycophancy");
#ifdef HU_ENABLE_SQLITE
    HU_RUN_TEST(find_returns_existing_opinion);
    HU_RUN_TEST(find_returns_not_found);
    HU_RUN_TEST(find_null_args_returns_error);
    HU_RUN_TEST(check_before_agree_strong_opinion_injects_directive);
    HU_RUN_TEST(check_before_agree_weak_opinion_returns_null);
    HU_RUN_TEST(check_before_agree_no_opinion_returns_null);
    HU_RUN_TEST(check_before_agree_null_args_returns_null);
    HU_RUN_TEST(contrarian_fires_approximately_15_percent);
    HU_RUN_TEST(contrarian_null_args_returns_null);
#endif
}
