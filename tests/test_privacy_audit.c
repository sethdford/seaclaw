/*
 * Phase 6 Privacy Firewall Audit Tests
 *
 * Ensures contact A's information is NEVER referenced in conversation with contact B.
 * Cross-contact isolation: memory scoped to session_id/contact_id.
 */
#include "human/core/allocator.h"
#include "human/memory.h"
#include "test_framework.h"
#include <string.h>

#ifdef HU_ENABLE_SQLITE

static void cross_contact_isolation_a_memory_not_in_b_context(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.ctx);

    /* Store memory for contact A */
    const char *key_a = "secret_a";
    const char *content_a = "A's secret info";
    const char *session_a = "contact_a";
    hu_memory_category_t cat = {.tag = HU_MEMORY_CATEGORY_CONVERSATION};
    hu_error_t err = mem.vtable->store(mem.ctx, key_a, strlen(key_a), content_a, strlen(content_a),
                                        &cat, session_a, strlen(session_a));
    HU_ASSERT_EQ(err, HU_OK);

    /* Build context for contact B: recall with session_id = contact_b */
    hu_memory_entry_t *entries = NULL;
    size_t count = 0;
    const char *session_b = "contact_b";
    const char *query = "secret";
    err = mem.vtable->recall(mem.ctx, &alloc, query, strlen(query), 10, session_b, strlen(session_b),
                             &entries, &count);

    /* A's memory must NOT appear in B's context */
    bool found_a_content = false;
    if (err == HU_OK && entries) {
        for (size_t i = 0; i < count; i++) {
            if (entries[i].content && entries[i].content_len >= 15 &&
                strstr(entries[i].content, "A's secret info") != NULL) {
                found_a_content = true;
                break;
            }
        }
        for (size_t i = 0; i < count; i++)
            hu_memory_entry_free_fields(&alloc, &entries[i]);
        alloc.free(alloc.ctx, entries, count * sizeof(hu_memory_entry_t));
    }
    HU_ASSERT_FALSE(found_a_content);

    /* Sanity: recall for contact A should return A's memory */
    entries = NULL;
    count = 0;
    err = mem.vtable->recall(mem.ctx, &alloc, query, strlen(query), 10, session_a, strlen(session_a),
                             &entries, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(count >= 1);
    bool found_for_a = false;
    if (entries) {
        for (size_t i = 0; i < count; i++) {
            if (entries[i].content && strstr(entries[i].content, "A's secret info") != NULL) {
                found_for_a = true;
                break;
            }
        }
        for (size_t i = 0; i < count; i++)
            hu_memory_entry_free_fields(&alloc, &entries[i]);
        alloc.free(alloc.ctx, entries, count * sizeof(hu_memory_entry_t));
    }
    HU_ASSERT_TRUE(found_for_a);

    mem.vtable->deinit(mem.ctx);
}

static void cross_contact_forget_does_not_affect_other_contact(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.ctx);

    hu_memory_category_t cat = {.tag = HU_MEMORY_CATEGORY_CONVERSATION};
    const char *key_a = "contact_a:item1";
    const char *key_b = "contact_b:item1";
    HU_ASSERT_EQ(mem.vtable->store(mem.ctx, key_a, strlen(key_a), "data-a", 6, &cat, "a", 1),
                 HU_OK);
    HU_ASSERT_EQ(mem.vtable->store(mem.ctx, key_b, strlen(key_b), "data-b", 6, &cat, "b", 1),
                 HU_OK);

    bool deleted = false;
    HU_ASSERT_EQ(mem.vtable->forget(mem.ctx, key_a, strlen(key_a), &deleted), HU_OK);
    HU_ASSERT_TRUE(deleted);

    hu_memory_entry_t entry;
    bool found = false;
    HU_ASSERT_EQ(mem.vtable->get(mem.ctx, &alloc, key_b, strlen(key_b), &entry, &found), HU_OK);
    HU_ASSERT_TRUE(found);
    hu_memory_entry_free_fields(&alloc, &entry);

    mem.vtable->deinit(mem.ctx);
}

static void cross_contact_list_shows_only_own_entries(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.ctx);

    hu_memory_category_t cat = {.tag = HU_MEMORY_CATEGORY_CORE};
    HU_ASSERT_EQ(mem.vtable->store(mem.ctx, "k1", 2, "val-a", 5, &cat, "alice", 5), HU_OK);
    HU_ASSERT_EQ(mem.vtable->store(mem.ctx, "k2", 2, "val-b", 5, &cat, "bob", 3), HU_OK);
    HU_ASSERT_EQ(mem.vtable->store(mem.ctx, "k3", 2, "val-a2", 6, &cat, "alice", 5), HU_OK);

    hu_memory_entry_t *entries = NULL;
    size_t count = 0;
    HU_ASSERT_EQ(mem.vtable->list(mem.ctx, &alloc, NULL, "alice", 5, &entries, &count), HU_OK);
    HU_ASSERT_EQ(count, 2u);
    for (size_t i = 0; i < count; i++)
        hu_memory_entry_free_fields(&alloc, &entries[i]);
    if (entries)
        alloc.free(alloc.ctx, entries, count * sizeof(hu_memory_entry_t));

    mem.vtable->deinit(mem.ctx);
}

static void cross_contact_count_reflects_all_contacts(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.ctx);

    hu_memory_category_t cat = {.tag = HU_MEMORY_CATEGORY_CORE};
    HU_ASSERT_EQ(mem.vtable->store(mem.ctx, "x1", 2, "v1", 2, &cat, "a", 1), HU_OK);
    HU_ASSERT_EQ(mem.vtable->store(mem.ctx, "x2", 2, "v2", 2, &cat, "b", 1), HU_OK);

    size_t total = 0;
    HU_ASSERT_EQ(mem.vtable->count(mem.ctx, &total), HU_OK);
    HU_ASSERT_EQ(total, 2u);

    mem.vtable->deinit(mem.ctx);
}

void run_privacy_audit_tests(void) {
    HU_TEST_SUITE("privacy_audit");
    HU_RUN_TEST(cross_contact_isolation_a_memory_not_in_b_context);
    HU_RUN_TEST(cross_contact_forget_does_not_affect_other_contact);
    HU_RUN_TEST(cross_contact_list_shows_only_own_entries);
    HU_RUN_TEST(cross_contact_count_reflects_all_contacts);
}

#else

void run_privacy_audit_tests(void) {
    HU_TEST_SUITE("privacy_audit");
    /* Cross-contact isolation test requires SQLite memory */
}

#endif /* HU_ENABLE_SQLITE */
