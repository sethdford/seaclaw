#include "test_framework.h"
#include "human/memory/corrective_rag.h"

static void test_crag_grade_relevant(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_rag_graded_doc_t doc;
    HU_ASSERT_EQ(hu_crag_grade_document(&alloc, "machine learning", 16, "deep machine learning models", 28, &doc), HU_OK);
    HU_ASSERT(doc.score > 0.0);
}

static void test_crag_grade_irrelevant(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_rag_graded_doc_t doc;
    HU_ASSERT_EQ(hu_crag_grade_document(&alloc, "quantum physics", 15, "cooking recipes for pasta", 25, &doc), HU_OK);
    HU_ASSERT_EQ((int)doc.relevance, (int)HU_RAG_IRRELEVANT);
}

static void test_crag_null(void) {
    hu_rag_graded_doc_t doc;
    HU_ASSERT_EQ(hu_crag_grade_document(NULL, "q", 1, "d", 1, &doc), HU_ERR_INVALID_ARGUMENT);
}

static void test_crag_null_query(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_rag_graded_doc_t doc;
    HU_ASSERT_EQ(hu_crag_grade_document(&alloc, NULL, 0, "doc", 3, &doc), HU_ERR_INVALID_ARGUMENT);
}

static void test_crag_null_doc(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_rag_graded_doc_t doc;
    HU_ASSERT_EQ(hu_crag_grade_document(&alloc, "query", 5, NULL, 0, &doc), HU_ERR_INVALID_ARGUMENT);
}

static void test_crag_empty_query(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_rag_graded_doc_t doc;
    HU_ASSERT_EQ(hu_crag_grade_document(&alloc, "", 0, "hello world", 11, &doc), HU_OK);
    HU_ASSERT_EQ(doc.score, 0.0);
    HU_ASSERT_EQ((int)doc.relevance, (int)HU_RAG_IRRELEVANT);
}

static void test_crag_exact_match(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *text = "machine learning";
    size_t len = 16;
    hu_rag_graded_doc_t doc;
    HU_ASSERT_EQ(hu_crag_grade_document(&alloc, text, len, text, len, &doc), HU_OK);
    HU_ASSERT_EQ(doc.score, 1.0);
    HU_ASSERT_EQ((int)doc.relevance, (int)HU_RAG_RELEVANT);
}

static void test_crag_partial_overlap(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_rag_graded_doc_t doc;
    HU_ASSERT_EQ(hu_crag_grade_document(&alloc, "alpha beta gamma delta epsilon", 30, "alpha unrelated text", 20, &doc), HU_OK);
    HU_ASSERT_EQ((int)doc.relevance, (int)HU_RAG_AMBIGUOUS);
}

static void test_crag_retrieve_null_config(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_crag_result_t out = {0};
    HU_ASSERT_EQ(hu_crag_retrieve(&alloc, NULL, "query", 5, &out), HU_ERR_INVALID_ARGUMENT);
}

static void test_crag_result_free_null_safe(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_crag_result_free(&alloc, NULL);
}

void run_corrective_rag_tests(void) {
    HU_TEST_SUITE("Corrective RAG");
    HU_RUN_TEST(test_crag_grade_relevant);
    HU_RUN_TEST(test_crag_grade_irrelevant);
    HU_RUN_TEST(test_crag_null);
    HU_RUN_TEST(test_crag_null_query);
    HU_RUN_TEST(test_crag_null_doc);
    HU_RUN_TEST(test_crag_empty_query);
    HU_RUN_TEST(test_crag_exact_match);
    HU_RUN_TEST(test_crag_partial_overlap);
    HU_RUN_TEST(test_crag_retrieve_null_config);
    HU_RUN_TEST(test_crag_result_free_null_safe);
}
