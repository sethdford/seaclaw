#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/skill_scaffold.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int s_tests_run = 0;
static int s_tests_passed = 0;

#define RUN(fn)           \
    do {                  \
        s_tests_run++;    \
        fn();             \
        s_tests_passed++; \
    } while (0)

static void *test_alloc(void *ctx, size_t size) {
    (void)ctx;
    return malloc(size);
}
static void test_free(void *ctx, void *ptr, size_t size) {
    (void)ctx;
    (void)size;
    free(ptr);
}
static hu_allocator_t s_alloc = {.alloc = test_alloc, .free = test_free};

static void test_manifest_basic(void) {
    hu_skill_scaffold_opts_t opts = {
        .name = "my-skill",
        .description = "A test skill",
        .author = "tester",
        .category = HU_SKILL_CATEGORY_GENERAL,
    };
    char *out = NULL;
    size_t len = 0;
    assert(hu_skill_scaffold_manifest(&s_alloc, &opts, &out, &len) == HU_OK);
    assert(out != NULL);
    assert(len > 0);
    assert(strstr(out, "\"my-skill\"") != NULL);
    assert(strstr(out, "\"A test skill\"") != NULL);
    assert(strstr(out, "\"tester\"") != NULL);
    assert(strstr(out, "\"general\"") != NULL);
    assert(strstr(out, "\"0.1.0\"") != NULL);
    s_alloc.free(s_alloc.ctx, out, len + 1);
}

static void test_manifest_data_category(void) {
    hu_skill_scaffold_opts_t opts = {
        .name = "data-fetcher",
        .category = HU_SKILL_CATEGORY_DATA,
    };
    char *out = NULL;
    assert(hu_skill_scaffold_manifest(&s_alloc, &opts, &out, NULL) == HU_OK);
    assert(strstr(out, "\"data\"") != NULL);
    s_alloc.free(s_alloc.ctx, out, strlen(out) + 1);
}

static void test_manifest_null_args(void) {
    char *out = NULL;
    hu_skill_scaffold_opts_t opts = {.name = "test"};
    assert(hu_skill_scaffold_manifest(NULL, &opts, &out, NULL) == HU_ERR_INVALID_ARGUMENT);
    assert(hu_skill_scaffold_manifest(&s_alloc, NULL, &out, NULL) == HU_ERR_INVALID_ARGUMENT);
    assert(hu_skill_scaffold_manifest(&s_alloc, &opts, NULL, NULL) == HU_ERR_INVALID_ARGUMENT);
    opts.name = NULL;
    assert(hu_skill_scaffold_manifest(&s_alloc, &opts, &out, NULL) == HU_ERR_INVALID_ARGUMENT);
}

static void test_instructions_basic(void) {
    hu_skill_scaffold_opts_t opts = {
        .name = "my-skill",
        .description = "Helpful skill",
    };
    char *out = NULL;
    size_t len = 0;
    assert(hu_skill_scaffold_instructions(&s_alloc, &opts, &out, &len) == HU_OK);
    assert(out != NULL);
    assert(len > 0);
    assert(strstr(out, "# my-skill") != NULL);
    assert(strstr(out, "Helpful skill") != NULL);
    assert(strstr(out, "## When to Use") != NULL);
    assert(strstr(out, "## Instructions") != NULL);
    assert(strstr(out, "## Examples") != NULL);
    assert(strstr(out, "## Constraints") != NULL);
    s_alloc.free(s_alloc.ctx, out, len + 1);
}

static void test_instructions_has_frontmatter(void) {
    hu_skill_scaffold_opts_t opts = {.name = "fm-test"};
    char *out = NULL;
    assert(hu_skill_scaffold_instructions(&s_alloc, &opts, &out, NULL) == HU_OK);
    assert(strncmp(out, "---\n", 4) == 0);
    assert(strstr(out, "name: fm-test") != NULL);
    assert(strstr(out, "version: 0.1.0") != NULL);
    s_alloc.free(s_alloc.ctx, out, strlen(out) + 1);
}

static void test_instructions_null_args(void) {
    char *out = NULL;
    hu_skill_scaffold_opts_t opts = {.name = "test"};
    assert(hu_skill_scaffold_instructions(NULL, &opts, &out, NULL) == HU_ERR_INVALID_ARGUMENT);
    assert(hu_skill_scaffold_instructions(&s_alloc, NULL, &out, NULL) == HU_ERR_INVALID_ARGUMENT);
    assert(hu_skill_scaffold_instructions(&s_alloc, &opts, NULL, NULL) == HU_ERR_INVALID_ARGUMENT);
}

static void test_init_validates_name(void) {
    hu_skill_scaffold_opts_t opts = {.name = NULL};
    assert(hu_skill_scaffold_init(&s_alloc, &opts) == HU_ERR_INVALID_ARGUMENT);

    opts.name = "";
    assert(hu_skill_scaffold_init(&s_alloc, &opts) == HU_ERR_INVALID_ARGUMENT);
}

static void test_init_success(void) {
    hu_skill_scaffold_opts_t opts = {
        .name = "test-skill",
        .description = "A scaffold test",
        .author = "bot",
        .category = HU_SKILL_CATEGORY_AUTOMATION,
    };
    assert(hu_skill_scaffold_init(&s_alloc, &opts) == HU_OK);
}

static void test_init_null_alloc(void) {
    hu_skill_scaffold_opts_t opts = {.name = "test"};
    assert(hu_skill_scaffold_init(NULL, &opts) == HU_ERR_INVALID_ARGUMENT);
}

static void test_init_null_opts(void) {
    assert(hu_skill_scaffold_init(&s_alloc, NULL) == HU_ERR_INVALID_ARGUMENT);
}

static void test_manifest_all_categories(void) {
    hu_skill_category_t cats[] = {
        HU_SKILL_CATEGORY_GENERAL,     HU_SKILL_CATEGORY_DATA,     HU_SKILL_CATEGORY_AUTOMATION,
        HU_SKILL_CATEGORY_INTEGRATION, HU_SKILL_CATEGORY_ANALYSIS,
    };
    const char *expected[] = {"general", "data", "automation", "integration", "analysis"};
    for (int i = 0; i < 5; i++) {
        hu_skill_scaffold_opts_t opts = {.name = "cat-test", .category = cats[i]};
        char *out = NULL;
        assert(hu_skill_scaffold_manifest(&s_alloc, &opts, &out, NULL) == HU_OK);
        assert(strstr(out, expected[i]) != NULL);
        s_alloc.free(s_alloc.ctx, out, strlen(out) + 1);
    }
}

static void test_manifest_default_description(void) {
    hu_skill_scaffold_opts_t opts = {.name = "bare"};
    char *out = NULL;
    assert(hu_skill_scaffold_manifest(&s_alloc, &opts, &out, NULL) == HU_OK);
    assert(strstr(out, "A custom human skill") != NULL);
    s_alloc.free(s_alloc.ctx, out, strlen(out) + 1);
}

int run_skill_scaffold_tests(void) {
    s_tests_run = 0;
    s_tests_passed = 0;

    RUN(test_manifest_basic);
    RUN(test_manifest_data_category);
    RUN(test_manifest_null_args);
    RUN(test_instructions_basic);
    RUN(test_instructions_has_frontmatter);
    RUN(test_instructions_null_args);
    RUN(test_init_validates_name);
    RUN(test_init_success);
    RUN(test_init_null_alloc);
    RUN(test_init_null_opts);
    RUN(test_manifest_all_categories);
    RUN(test_manifest_default_description);

    printf("  skill_scaffold: %d/%d passed\n", s_tests_passed, s_tests_run);
    return s_tests_run - s_tests_passed;
}
