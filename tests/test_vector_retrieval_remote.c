#include "human/core/allocator.h"
#include "human/memory/vector.h"
#include "test_framework.h"
#include <string.h>

hu_vector_store_t hu_vector_store_qdrant_retrieval_create(hu_allocator_t *alloc, const char *url,
                                                          const char *api_key,
                                                          const char *collection,
                                                          size_t dimensions);

static void vector_retrieval_remote_qdrant_create(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_vector_store_t store =
        hu_vector_store_qdrant_retrieval_create(&alloc, "http://localhost:6333", NULL, "test", 384);
    HU_ASSERT_NOT_NULL(store.vtable);
    HU_ASSERT_NOT_NULL(store.ctx);
    HU_ASSERT_EQ(store.vtable->count(store.ctx), 0);
    store.vtable->deinit(store.ctx, &alloc);
}

static void vector_retrieval_remote_qdrant_insert_search(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_vector_store_t store =
        hu_vector_store_qdrant_retrieval_create(&alloc, "http://localhost:6333", NULL, "test", 4);
    float vals[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    hu_embedding_t emb = {.values = vals, .dim = 4};
    HU_ASSERT_EQ(store.vtable->insert(store.ctx, &alloc, "id1", 3, &emb, "hello", 5), HU_OK);
    hu_vector_entry_t *results = NULL;
    size_t count = 0;
    HU_ASSERT_EQ(store.vtable->search(store.ctx, &alloc, &emb, 5, &results, &count), HU_OK);
    store.vtable->deinit(store.ctx, &alloc);
}

static void vector_retrieval_remote_qdrant_remove(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_vector_store_t store =
        hu_vector_store_qdrant_retrieval_create(&alloc, "http://localhost:6333", NULL, "test", 4);
    HU_ASSERT_EQ(store.vtable->remove(store.ctx, "id1", 3), HU_OK);
    store.vtable->deinit(store.ctx, &alloc);
}

void run_vector_retrieval_remote_tests(void) {
    HU_TEST_SUITE("VectorRetrievalRemote");
    HU_RUN_TEST(vector_retrieval_remote_qdrant_create);
    HU_RUN_TEST(vector_retrieval_remote_qdrant_insert_search);
    HU_RUN_TEST(vector_retrieval_remote_qdrant_remove);
}
