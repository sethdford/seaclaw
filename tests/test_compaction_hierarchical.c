#include "human/agent/compaction.h"
#include "human/core/allocator.h"
#include "test_framework.h"
#include <stddef.h>
#include <string.h>

static void compact_hierarchical_returns_mock_summaries(void) {
    hu_allocator_t alloc = hu_system_allocator();

    char *sess = NULL;
    size_t sess_len = 0;
    char *chap = NULL;
    size_t chap_len = 0;
    char *overall = NULL;
    size_t overall_len = 0;

    HU_ASSERT_EQ(hu_compact_hierarchical(&alloc, NULL, NULL, 0, "user: hi", 8, &sess, &sess_len,
                                         &chap, &chap_len, &overall, &overall_len),
                 HU_OK);
    HU_ASSERT_NOT_NULL(sess);
    HU_ASSERT_NOT_NULL(chap);
    HU_ASSERT_NOT_NULL(overall);
    HU_ASSERT_STR_EQ(sess, "Session summary: discussed project plans and timeline");
    HU_ASSERT_STR_EQ(chap, "Chapter summary: ongoing project work, key decisions made");
    HU_ASSERT_STR_EQ(overall, "Overall: productive collaboration on software project");
    HU_ASSERT_EQ(sess_len, strlen(sess));
    HU_ASSERT_EQ(chap_len, strlen(chap));
    HU_ASSERT_EQ(overall_len, strlen(overall));

    alloc.free(alloc.ctx, sess, sess_len + 1);
    alloc.free(alloc.ctx, chap, chap_len + 1);
    alloc.free(alloc.ctx, overall, overall_len + 1);
}

static void compact_hierarchical_rejects_null_allocator(void) {
    char *sess = NULL;
    size_t sess_len = 0;
    char *chap = NULL;
    size_t chap_len = 0;
    char *overall = NULL;
    size_t overall_len = 0;

    HU_ASSERT_EQ(hu_compact_hierarchical(NULL, NULL, NULL, 0, "", 0, &sess, &sess_len, &chap,
                                         &chap_len, &overall, &overall_len),
                 HU_ERR_INVALID_ARGUMENT);
}

static void compact_hierarchical_rejects_null_outputs(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *sess = NULL;
    size_t sess_len = 0;
    char *chap = NULL;
    size_t chap_len = 0;
    char *overall = NULL;
    size_t overall_len = 0;

    HU_ASSERT_EQ(hu_compact_hierarchical(&alloc, NULL, NULL, 0, "", 0, NULL, &sess_len, &chap,
                                         &chap_len, &overall, &overall_len),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_compact_hierarchical(&alloc, NULL, NULL, 0, "", 0, &sess, NULL, &chap,
                                         &chap_len, &overall, &overall_len),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_compact_hierarchical(&alloc, NULL, NULL, 0, "", 0, &sess, &sess_len, NULL,
                                         &chap_len, &overall, &overall_len),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_compact_hierarchical(&alloc, NULL, NULL, 0, "", 0, &sess, &sess_len, &chap,
                                         NULL, &overall, &overall_len),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_compact_hierarchical(&alloc, NULL, NULL, 0, "", 0, &sess, &sess_len, &chap,
                                         &chap_len, NULL, &overall_len),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_compact_hierarchical(&alloc, NULL, NULL, 0, "", 0, &sess, &sess_len, &chap,
                                         &chap_len, &overall, NULL),
                 HU_ERR_INVALID_ARGUMENT);
}

static void compact_hierarchical_rejects_null_conversation_with_nonzero_len(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *sess = NULL;
    size_t sess_len = 0;
    char *chap = NULL;
    size_t chap_len = 0;
    char *overall = NULL;
    size_t overall_len = 0;

    HU_ASSERT_EQ(hu_compact_hierarchical(&alloc, NULL, NULL, 0, NULL, 5, &sess, &sess_len, &chap,
                                         &chap_len, &overall, &overall_len),
                 HU_ERR_INVALID_ARGUMENT);
}

static void compact_hierarchical_allows_empty_conversation(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *sess = NULL;
    size_t sess_len = 0;
    char *chap = NULL;
    size_t chap_len = 0;
    char *overall = NULL;
    size_t overall_len = 0;

    HU_ASSERT_EQ(hu_compact_hierarchical(&alloc, NULL, NULL, 0, NULL, 0, &sess, &sess_len, &chap,
                                         &chap_len, &overall, &overall_len),
                 HU_OK);
    HU_ASSERT_NOT_NULL(sess);
    alloc.free(alloc.ctx, sess, sess_len + 1);
    alloc.free(alloc.ctx, chap, chap_len + 1);
    alloc.free(alloc.ctx, overall, overall_len + 1);
}

void run_compaction_hierarchical_tests(void) {
    HU_TEST_SUITE("compaction_hierarchical");
    HU_RUN_TEST(compact_hierarchical_rejects_null_allocator);
    HU_RUN_TEST(compact_hierarchical_returns_mock_summaries);
    HU_RUN_TEST(compact_hierarchical_rejects_null_outputs);
    HU_RUN_TEST(compact_hierarchical_rejects_null_conversation_with_nonzero_len);
    HU_RUN_TEST(compact_hierarchical_allows_empty_conversation);
}
