#include "test_framework.h"
#include "human/core/allocator.h"
#include "human/memory/contact_graph.h"

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

#endif

void run_contact_graph_tests(void) {
#if defined(HU_ENABLE_SQLITE)
    HU_TEST_SUITE("contact_graph");
    HU_RUN_TEST(contact_graph_link_resolve_round_trip);
#else
    (void)0;
#endif
}
