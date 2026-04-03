#include "human/daemon.h"
#include "human/intelligence/trust.h"
#include "test_framework.h"
#include <string.h>
#include <stdio.h>

/* ── hu_daemon_get_trust_state ──────────────────────────────────────── */

static void test_trust_state_null_returns_null(void) {
    HU_ASSERT_NULL(hu_daemon_get_trust_state(NULL, 0));
    HU_ASSERT_NULL(hu_daemon_get_trust_state("abc", 0));
}

static void test_trust_state_creates_new_entry(void) {
    hu_daemon_trust_reset();
    hu_trust_state_t *ts = hu_daemon_get_trust_state("alice", 5);
    HU_ASSERT_NOT_NULL(ts);
    HU_ASSERT_EQ(hu_daemon_trust_count(), (size_t)1);
    /* Verify initialized to neutral */
    HU_ASSERT(ts->trust_level >= 0.49 && ts->trust_level <= 0.51);
}

static void test_trust_state_lookup_existing(void) {
    hu_daemon_trust_reset();
    hu_trust_state_t *ts1 = hu_daemon_get_trust_state("bob", 3);
    HU_ASSERT_NOT_NULL(ts1);
    /* Modify trust level to verify we get the same entry */
    ts1->trust_level = 0.75;
    hu_trust_state_t *ts2 = hu_daemon_get_trust_state("bob", 3);
    HU_ASSERT_NOT_NULL(ts2);
    HU_ASSERT(ts2->trust_level >= 0.74 && ts2->trust_level <= 0.76);
    /* Count should still be 1 */
    HU_ASSERT_EQ(hu_daemon_trust_count(), (size_t)1);
}

static void test_trust_state_distinct_contacts(void) {
    hu_daemon_trust_reset();
    hu_trust_state_t *a = hu_daemon_get_trust_state("user_a", 6);
    hu_trust_state_t *b = hu_daemon_get_trust_state("user_b", 6);
    HU_ASSERT_NOT_NULL(a);
    HU_ASSERT_NOT_NULL(b);
    HU_ASSERT(a != b);
    HU_ASSERT_EQ(hu_daemon_trust_count(), (size_t)2);
}

static void test_trust_state_long_id_truncated(void) {
    hu_daemon_trust_reset();
    /* 200-byte contact id exceeds the 128-byte buffer — should truncate */
    char long_id[200];
    memset(long_id, 'x', sizeof(long_id));
    hu_trust_state_t *ts = hu_daemon_get_trust_state(long_id, sizeof(long_id));
    HU_ASSERT_NOT_NULL(ts);
    HU_ASSERT_EQ(hu_daemon_trust_count(), (size_t)1);
}

/* ── LRU eviction ───────────────────────────────────────────────────── */

static void test_trust_state_evicts_lru_when_full(void) {
    hu_daemon_trust_reset();
    /* Fill the cache with 4096 entries (the cap) */
    char id_buf[32];
    for (int i = 0; i < 4096; i++) {
        int len = snprintf(id_buf, sizeof(id_buf), "contact_%d", i);
        hu_trust_state_t *ts = hu_daemon_get_trust_state(id_buf, (size_t)len);
        HU_ASSERT_NOT_NULL(ts);
        /* Set last_updated_at so we know which is oldest.
         * Contact 0 gets timestamp 1000, contact 1 gets 1001, etc. */
        ts->last_updated_at = 1000 + i;
    }
    HU_ASSERT_EQ(hu_daemon_trust_count(), (size_t)4096);

    /* Add one more — should evict contact_0 (oldest timestamp = 1000) */
    hu_trust_state_t *ts_new = hu_daemon_get_trust_state("contact_new", 11);
    HU_ASSERT_NOT_NULL(ts_new);
    /* Count stays at 4096 */
    HU_ASSERT_EQ(hu_daemon_trust_count(), (size_t)4096);

    /* contact_0 should no longer exist — looking it up creates a new entry
     * which evicts the next oldest (contact_1, timestamp 1001).
     * The returned state should be freshly initialized (trust_level ~0.5). */
    hu_trust_state_t *evicted = hu_daemon_get_trust_state("contact_0", 9);
    HU_ASSERT_NOT_NULL(evicted);
    HU_ASSERT(evicted->trust_level >= 0.49 && evicted->trust_level <= 0.51);
}

static void test_trust_state_no_eviction_below_cap(void) {
    hu_daemon_trust_reset();
    /* Add 10 contacts — no eviction should occur */
    char id_buf[32];
    for (int i = 0; i < 10; i++) {
        int len = snprintf(id_buf, sizeof(id_buf), "user_%d", i);
        hu_trust_state_t *ts = hu_daemon_get_trust_state(id_buf, (size_t)len);
        HU_ASSERT_NOT_NULL(ts);
        ts->trust_level = 0.1 * i;
    }
    HU_ASSERT_EQ(hu_daemon_trust_count(), (size_t)10);
    /* All entries still accessible */
    hu_trust_state_t *ts5 = hu_daemon_get_trust_state("user_5", 6);
    HU_ASSERT_NOT_NULL(ts5);
    HU_ASSERT(ts5->trust_level >= 0.49 && ts5->trust_level <= 0.51);
}

/* ── Reset helper ───────────────────────────────────────────────────── */

static void test_trust_reset_clears_all(void) {
    hu_daemon_trust_reset();
    hu_daemon_get_trust_state("test_contact", 12);
    HU_ASSERT_EQ(hu_daemon_trust_count(), (size_t)1);
    hu_daemon_trust_reset();
    HU_ASSERT_EQ(hu_daemon_trust_count(), (size_t)0);
}

void run_daemon_trust_tests(void) {
    HU_TEST_SUITE("daemon_trust");

    /* basic operations */
    HU_RUN_TEST(test_trust_state_null_returns_null);
    HU_RUN_TEST(test_trust_state_creates_new_entry);
    HU_RUN_TEST(test_trust_state_lookup_existing);
    HU_RUN_TEST(test_trust_state_distinct_contacts);
    HU_RUN_TEST(test_trust_state_long_id_truncated);

    /* LRU eviction */
    HU_RUN_TEST(test_trust_state_evicts_lru_when_full);
    HU_RUN_TEST(test_trust_state_no_eviction_below_cap);

    /* reset */
    HU_RUN_TEST(test_trust_reset_clears_all);
}
