#include "human/core/allocator.h"
#include "human/persona.h"
#include "human/persona/markdown_loader.h"
#include "test_framework.h"
#include <string.h>

static void markdown_load_returns_not_supported_in_test(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_persona_t persona;
    memset(&persona, 0, sizeof(persona));
    HU_ASSERT_EQ(hu_persona_load_markdown(&alloc, "/tmp/example.md", &persona),
                 HU_ERR_NOT_SUPPORTED);
}

static void markdown_discover_returns_empty_in_test(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char **names = (char **)1; /* poison: should be cleared */
    size_t count = 99U;
    HU_ASSERT_EQ(hu_persona_discover_agents(&alloc, "/agents", &names, &count), HU_OK);
    HU_ASSERT_EQ((long long)count, 0LL);
    HU_ASSERT_NULL(names);
}

static void markdown_load_null_alloc_returns_error(void) {
    hu_persona_t persona;
    memset(&persona, 0, sizeof(persona));
    HU_ASSERT_EQ(hu_persona_load_markdown(NULL, "/any/path.md", &persona),
                 HU_ERR_INVALID_ARGUMENT);
}

static void markdown_discover_null_dir_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char **names = NULL;
    size_t count = 0U;
    HU_ASSERT_EQ(hu_persona_discover_agents(&alloc, NULL, &names, &count),
                 HU_ERR_INVALID_ARGUMENT);
}

void run_markdown_loader_tests(void) {
    HU_TEST_SUITE("markdown_loader");
    HU_RUN_TEST(markdown_load_returns_not_supported_in_test);
    HU_RUN_TEST(markdown_discover_returns_empty_in_test);
    HU_RUN_TEST(markdown_load_null_alloc_returns_error);
    HU_RUN_TEST(markdown_discover_null_dir_returns_error);
}
