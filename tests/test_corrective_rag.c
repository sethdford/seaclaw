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

void run_corrective_rag_tests(void) {
    HU_TEST_SUITE("Corrective RAG");
    HU_RUN_TEST(test_crag_grade_relevant);
    HU_RUN_TEST(test_crag_grade_irrelevant);
    HU_RUN_TEST(test_crag_null);
}
