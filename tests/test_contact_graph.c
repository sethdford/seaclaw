#include "test_framework.h"
#include "human/core/allocator.h"
#include "human/memory/contact_graph.h"

static void contact_normalize_phone_strips_punctuation(void) {
    char buf[64];
    size_t n = hu_contact_normalize_phone("(555) 123-4567", buf, sizeof(buf));
    HU_ASSERT_EQ(n, (size_t)10);
    HU_ASSERT_STR_EQ(buf, "5551234567");
}

static void contact_normalize_phone_preserves_leading_plus(void) {
    char buf[64];
    size_t n = hu_contact_normalize_phone("+1 (555) 000-1234", buf, sizeof(buf));
    HU_ASSERT_EQ(n, (size_t)12);
    HU_ASSERT_STR_EQ(buf, "+15550001234");
}

static void contact_normalize_phone_drops_plus_after_digits(void) {
    char buf[64];
    size_t n = hu_contact_normalize_phone("555+1234", buf, sizeof(buf));
    HU_ASSERT_EQ(n, (size_t)7);
    HU_ASSERT_STR_EQ(buf, "5551234");
}

static void contact_normalize_phone_null_or_zero_cap_returns_zero(void) {
    char buf[8];
    HU_ASSERT_EQ(hu_contact_normalize_phone(NULL, buf, sizeof(buf)), (size_t)0);
    HU_ASSERT_EQ(hu_contact_normalize_phone("1", NULL, sizeof(buf)), (size_t)0);
    HU_ASSERT_EQ(hu_contact_normalize_phone("1", buf, 0), (size_t)0);
}

#if defined(HU_ENABLE_SQLITE)
#include <sqlite3.h>

static void contact_graph_link_resolve_round_trip(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
    HU_ASSERT_EQ(hu_contact_graph_init(&alloc, db), HU_OK);
    HU_ASSERT_EQ(
        hu_contact_graph_link(db, "cid_a", "discord", "user#1", "Alice", 1.0), HU_OK);
    char out[128] = {0};
    HU_ASSERT_EQ(hu_contact_graph_resolve(db, "discord", "user#1", out, sizeof(out)), HU_OK);
    HU_ASSERT_STR_EQ(out, "cid_a");
    sqlite3_close(db);
}

static void contact_graph_auto_resolve_null_allocator_returns_error(void) {
    hu_contact_identity_t *cands = NULL;
    size_t count = 0;
    HU_ASSERT_EQ(hu_contact_graph_auto_resolve(NULL, NULL, NULL, NULL, &cands, &count),
        HU_ERR_INVALID_ARGUMENT);
}

static void contact_graph_auto_resolve_null_out_pointers_return_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_contact_identity_t *cands = NULL;
    size_t count = 0;
    HU_ASSERT_EQ(
        hu_contact_graph_auto_resolve(&alloc, NULL, "", "", NULL, &count), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(
        hu_contact_graph_auto_resolve(&alloc, NULL, "", "", &cands, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void contact_graph_auto_resolve_empty_db_returns_ok(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
    HU_ASSERT_EQ(hu_contact_graph_init(&alloc, db), HU_OK);
    hu_contact_identity_t *cands = NULL;
    size_t count = 0;
    HU_ASSERT_EQ(hu_contact_graph_auto_resolve(
                     &alloc, db, "Alice", "+1 555-0100", &cands, &count),
        HU_OK);
    HU_ASSERT_EQ(count, (size_t)0);
    HU_ASSERT_NULL(cands);
    sqlite3_close(db);
}

/* With HU_IS_TEST=1 (human_tests), auto-resolve skips SQL; this asserts that contract. */
static void contact_graph_auto_resolve_test_mode_returns_no_candidates(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
    HU_ASSERT_EQ(hu_contact_graph_init(&alloc, db), HU_OK);
    HU_ASSERT_EQ(hu_contact_graph_link(db, "cid_x", "slack", "handle_x", "Bob", 1.0), HU_OK);
    hu_contact_identity_t *cands = NULL;
    size_t count = 0;
    HU_ASSERT_EQ(hu_contact_graph_auto_resolve(
                     &alloc, db, "Bob", "handle_x", &cands, &count),
        HU_OK);
#if defined(HU_IS_TEST) && HU_IS_TEST
    HU_ASSERT_EQ(count, (size_t)0);
    HU_ASSERT_NULL(cands);
#else
    HU_ASSERT_GT(count, (size_t)0);
    HU_ASSERT_NOT_NULL(cands);
    alloc.free(alloc.ctx, cands, count * sizeof(hu_contact_identity_t));
#endif
    sqlite3_close(db);
}

#endif

void run_contact_graph_tests(void) {
    HU_TEST_SUITE("contact_graph");
    HU_RUN_TEST(contact_normalize_phone_strips_punctuation);
    HU_RUN_TEST(contact_normalize_phone_preserves_leading_plus);
    HU_RUN_TEST(contact_normalize_phone_drops_plus_after_digits);
    HU_RUN_TEST(contact_normalize_phone_null_or_zero_cap_returns_zero);
#if defined(HU_ENABLE_SQLITE)
    HU_RUN_TEST(contact_graph_link_resolve_round_trip);
    HU_RUN_TEST(contact_graph_auto_resolve_null_allocator_returns_error);
    HU_RUN_TEST(contact_graph_auto_resolve_null_out_pointers_return_error);
    HU_RUN_TEST(contact_graph_auto_resolve_empty_db_returns_ok);
    HU_RUN_TEST(contact_graph_auto_resolve_test_mode_returns_no_candidates);
#endif
}
