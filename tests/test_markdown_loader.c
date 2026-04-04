#include "human/core/allocator.h"
#include "human/persona.h"
#include "human/persona/markdown_loader.h"
#include "test_framework.h"

#include <string.h>

extern hu_error_t hu_persona_load_markdown_buffer(hu_allocator_t *alloc, const char *content,
                                                  size_t content_len, hu_persona_t *out);

static void test_markdown_loader_path_unsupported_under_test(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);
    hu_persona_t p;
    memset(&p, 0, sizeof(p));
    HU_ASSERT_EQ(hu_persona_load_markdown(&alloc, "/nonexistent/agents/x.md", &p), HU_ERR_NOT_SUPPORTED);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

static void test_markdown_loader_parses_frontmatter_and_body(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);
    hu_persona_t p;
    memset(&p, 0, sizeof(p));

    static const char doc[] = "---\n"
                              "name: test_agent\n"
                              "identity: Short intro.\n"
                              "model: gemini-3-flash-preview\n"
                              "traits:\n"
                              "  - concise\n"
                              "  - kind\n"
                              "overlays:\n"
                              "  slack:\n"
                              "    formality: casual\n"
                              "    avg_length: short\n"
                              "---\n"
                              "\n"
                              "Extended **body** paragraph.\n";

    HU_ASSERT_EQ(hu_persona_load_markdown_buffer(&alloc, doc, strlen(doc), &p), HU_OK);
    HU_ASSERT_NOT_NULL(p.name);
    HU_ASSERT_STR_EQ(p.name, "test_agent");
    HU_ASSERT_EQ(p.name_len, strlen("test_agent"));
    HU_ASSERT_NOT_NULL(p.identity);
    HU_ASSERT_TRUE(strstr(p.identity, "Short intro.") != NULL);
    HU_ASSERT_TRUE(strstr(p.identity, "Extended **body** paragraph.") != NULL);
    HU_ASSERT_EQ(p.traits_count, 2U);
    HU_ASSERT_NOT_NULL(p.traits);
    HU_ASSERT_STR_EQ(p.traits[0], "concise");
    HU_ASSERT_STR_EQ(p.traits[1], "kind");
    HU_ASSERT_EQ(p.overlays_count, 1U);
    HU_ASSERT_NOT_NULL(p.overlays);
    HU_ASSERT_STR_EQ(p.overlays[0].channel, "slack");
    HU_ASSERT_NOT_NULL(p.overlays[0].formality);
    HU_ASSERT_STR_EQ(p.overlays[0].formality, "casual");
    HU_ASSERT_NOT_NULL(p.overlays[0].avg_length);
    HU_ASSERT_STR_EQ(p.overlays[0].avg_length, "short");
    HU_ASSERT_STR_EQ(p.voice.model, "gemini-3-flash-preview");

    hu_persona_deinit(&alloc, &p);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

static void test_markdown_loader_no_frontmatter_body_only_defaults(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);
    hu_persona_t p;
    memset(&p, 0, sizeof(p));

    static const char doc[] = "Just markdown body.\nNo YAML fence.\n";

    HU_ASSERT_EQ(hu_persona_load_markdown_buffer(&alloc, doc, strlen(doc), &p), HU_OK);
    HU_ASSERT_NULL(p.name);
    HU_ASSERT_NOT_NULL(p.identity);
    HU_ASSERT_STR_EQ(p.identity, "Just markdown body.\nNo YAML fence.");
    HU_ASSERT_EQ(p.traits_count, 0U);

    hu_persona_deinit(&alloc, &p);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

static void test_markdown_loader_empty_buffer_defaults(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);
    hu_persona_t p;
    memset(&p, 0xab, sizeof(p));

    HU_ASSERT_EQ(hu_persona_load_markdown_buffer(&alloc, NULL, 0, &p), HU_OK);
    HU_ASSERT_NULL(p.name);
    HU_ASSERT_NULL(p.identity);
    HU_ASSERT_EQ(p.traits_count, 0U);

    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

static void test_markdown_loader_discover_empty_under_test(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);
    char **names = NULL;
    size_t n = 0;
    HU_ASSERT_EQ(hu_persona_discover_agents(&alloc, "/tmp/human_agents_absent", &names, &n), HU_OK);
    HU_ASSERT_EQ(n, 0U);
    HU_ASSERT_NULL(names);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

void run_markdown_loader_tests(void) {
    HU_TEST_SUITE("markdown_loader");
    HU_RUN_TEST(test_markdown_loader_path_unsupported_under_test);
    HU_RUN_TEST(test_markdown_loader_parses_frontmatter_and_body);
    HU_RUN_TEST(test_markdown_loader_no_frontmatter_body_only_defaults);
    HU_RUN_TEST(test_markdown_loader_empty_buffer_defaults);
    HU_RUN_TEST(test_markdown_loader_discover_empty_under_test);
}
