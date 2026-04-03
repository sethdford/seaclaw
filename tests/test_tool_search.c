/* Tests for tool_search tool and workspace context detection */
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/json.h"
#include "human/tool.h"
#include "human/tools/tool_search.h"
#include "human/agent/workspace_context.h"
#include "test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if HU_IS_TEST
#include <sys/stat.h>
#include <unistd.h>
#endif

/* ──────────────────────────────────────────────────────────────────────────
 * Tool Search Tests
 * ────────────────────────────────────────────────────────────────────────── */

/* Mock tool implementations for testing */
static const char *mock_tool_name_1(void *ctx) {
    (void)ctx;
    return "file_read";
}

static const char *mock_tool_desc_1(void *ctx) {
    (void)ctx;
    return "Read files from the workspace";
}

static const char *mock_tool_params_1(void *ctx) {
    (void)ctx;
    return "{}";
}

static hu_error_t mock_tool_execute_1(void *ctx, hu_allocator_t *alloc, const hu_json_value_t *args,
                                      hu_tool_result_t *out) {
    (void)ctx;
    (void)alloc;
    (void)args;
    *out = hu_tool_result_ok("", 0);
    return HU_OK;
}

static const hu_tool_vtable_t mock_vtable_1 = {
    .name = mock_tool_name_1,
    .description = mock_tool_desc_1,
    .parameters_json = mock_tool_params_1,
    .execute = mock_tool_execute_1,
    .deinit = NULL,
};

static const char *mock_tool_name_2(void *ctx) {
    (void)ctx;
    return "web_search";
}

static const char *mock_tool_desc_2(void *ctx) {
    (void)ctx;
    return "Search the web for information";
}

static const char *mock_tool_params_2(void *ctx) {
    (void)ctx;
    return "{}";
}

static hu_error_t mock_tool_execute_2(void *ctx, hu_allocator_t *alloc, const hu_json_value_t *args,
                                      hu_tool_result_t *out) {
    (void)ctx;
    (void)alloc;
    (void)args;
    *out = hu_tool_result_ok("", 0);
    return HU_OK;
}

static const hu_tool_vtable_t mock_vtable_2 = {
    .name = mock_tool_name_2,
    .description = mock_tool_desc_2,
    .parameters_json = mock_tool_params_2,
    .execute = mock_tool_execute_2,
    .deinit = NULL,
};

static const char *mock_tool_name_3(void *ctx) {
    (void)ctx;
    return "shell";
}

static const char *mock_tool_desc_3(void *ctx) {
    (void)ctx;
    return "Execute shell commands";
}

static const char *mock_tool_params_3(void *ctx) {
    (void)ctx;
    return "{}";
}

static hu_error_t mock_tool_execute_3(void *ctx, hu_allocator_t *alloc, const hu_json_value_t *args,
                                      hu_tool_result_t *out) {
    (void)ctx;
    (void)alloc;
    (void)args;
    *out = hu_tool_result_ok("", 0);
    return HU_OK;
}

static const hu_tool_vtable_t mock_vtable_3 = {
    .name = mock_tool_name_3,
    .description = mock_tool_desc_3,
    .parameters_json = mock_tool_params_3,
    .execute = mock_tool_execute_3,
    .deinit = NULL,
};

static void test_tool_search_create(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tools[3];

    /* Create mock tools */
    tools[0].ctx = NULL;
    tools[0].vtable = &mock_vtable_1;
    tools[1].ctx = NULL;
    tools[1].vtable = &mock_vtable_2;
    tools[2].ctx = NULL;
    tools[2].vtable = &mock_vtable_3;

    hu_tool_t search_tool;
    hu_error_t err = hu_tool_search_create(&alloc, tools, 3, &search_tool);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(search_tool.ctx);
    HU_ASSERT_NOT_NULL(search_tool.vtable);

    if (search_tool.vtable->deinit)
        search_tool.vtable->deinit(search_tool.ctx, &alloc);
}

static void test_tool_search_name(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tools[1];
    tools[0].ctx = NULL;
    tools[0].vtable = &mock_vtable_1;

    hu_tool_t search_tool;
    hu_error_t err = hu_tool_search_create(&alloc, tools, 1, &search_tool);
    HU_ASSERT_EQ(err, HU_OK);

    const char *name = search_tool.vtable->name(search_tool.ctx);
    HU_ASSERT_NOT_NULL(name);
    HU_ASSERT_STR_EQ(name, "tool_search");

    if (search_tool.vtable->deinit)
        search_tool.vtable->deinit(search_tool.ctx, &alloc);
}

static void test_tool_search_description(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tools[1];
    tools[0].ctx = NULL;
    tools[0].vtable = &mock_vtable_1;

    hu_tool_t search_tool;
    hu_error_t err = hu_tool_search_create(&alloc, tools, 1, &search_tool);
    HU_ASSERT_EQ(err, HU_OK);

    const char *desc = search_tool.vtable->description(search_tool.ctx);
    HU_ASSERT_NOT_NULL(desc);
    HU_ASSERT_STR_EQ(desc, "Search available tools by name or keyword");

    if (search_tool.vtable->deinit)
        search_tool.vtable->deinit(search_tool.ctx, &alloc);
}

static void test_tool_search_parameters_json(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tools[1];
    tools[0].ctx = NULL;
    tools[0].vtable = &mock_vtable_1;

    hu_tool_t search_tool;
    hu_error_t err = hu_tool_search_create(&alloc, tools, 1, &search_tool);
    HU_ASSERT_EQ(err, HU_OK);

    const char *params = search_tool.vtable->parameters_json(search_tool.ctx);
    HU_ASSERT_NOT_NULL(params);
    HU_ASSERT_STR_CONTAINS(params, "query");
    HU_ASSERT_STR_CONTAINS(params, "string");

    if (search_tool.vtable->deinit)
        search_tool.vtable->deinit(search_tool.ctx, &alloc);
}

static void test_tool_search_execute_empty_tools(void) {
    hu_allocator_t alloc = hu_system_allocator();

    hu_tool_t search_tool;
    hu_error_t err = hu_tool_search_create(&alloc, NULL, 0, &search_tool);
    HU_ASSERT_EQ(err, HU_OK);

    /* Create a minimal JSON object with query field */
    hu_json_value_t query_val = {
        .type = HU_JSON_STRING,
        .data.string = {.ptr = "file", .len = 4},
    };

    hu_json_pair_t pairs[1] = {
        {.key = "query", .key_len = 5, .value = &query_val},
    };

    hu_json_value_t args = {
        .type = HU_JSON_OBJECT,
        .data.object = {.pairs = pairs, .len = 1, .cap = 1},
    };

    hu_tool_result_t result;
    err = search_tool.vtable->execute(search_tool.ctx, &alloc, &args, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.success);
    HU_ASSERT_STR_EQ(result.output, "[]");

    hu_tool_result_free(&alloc, &result);

    if (search_tool.vtable->deinit)
        search_tool.vtable->deinit(search_tool.ctx, &alloc);
}

static void test_tool_search_execute_match_by_name(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tools[3];

    tools[0].ctx = NULL;
    tools[0].vtable = &mock_vtable_1;
    tools[1].ctx = NULL;
    tools[1].vtable = &mock_vtable_2;
    tools[2].ctx = NULL;
    tools[2].vtable = &mock_vtable_3;

    hu_tool_t search_tool;
    hu_error_t err = hu_tool_search_create(&alloc, tools, 3, &search_tool);
    HU_ASSERT_EQ(err, HU_OK);

    /* Search for "web" — should match web_search */
    hu_json_value_t query_val = {
        .type = HU_JSON_STRING,
        .data.string = {.ptr = "web", .len = 3},
    };

    hu_json_pair_t pairs_web[1] = {
        {.key = "query", .key_len = 5, .value = &query_val},
    };

    hu_json_value_t args = {
        .type = HU_JSON_OBJECT,
        .data.object = {.pairs = pairs_web, .len = 1, .cap = 1},
    };

    hu_tool_result_t result;
    err = search_tool.vtable->execute(search_tool.ctx, &alloc, &args, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.success);
    HU_ASSERT_STR_CONTAINS(result.output, "web_search");
    HU_ASSERT_STR_CONTAINS(result.output, "Search the web");

    hu_tool_result_free(&alloc, &result);

    if (search_tool.vtable->deinit)
        search_tool.vtable->deinit(search_tool.ctx, &alloc);
}

static void test_tool_search_execute_match_by_description(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tools[3];

    tools[0].ctx = NULL;
    tools[0].vtable = &mock_vtable_1;
    tools[1].ctx = NULL;
    tools[1].vtable = &mock_vtable_2;
    tools[2].ctx = NULL;
    tools[2].vtable = &mock_vtable_3;

    hu_tool_t search_tool;
    hu_error_t err = hu_tool_search_create(&alloc, tools, 3, &search_tool);
    HU_ASSERT_EQ(err, HU_OK);

    /* Search for "command" — should match shell (Execute shell commands) */
    hu_json_value_t query_val = {
        .type = HU_JSON_STRING,
        .data.string = {.ptr = "command", .len = 7},
    };

    hu_json_pair_t pairs_cmd[1] = {
        {.key = "query", .key_len = 5, .value = &query_val},
    };

    hu_json_value_t args = {
        .type = HU_JSON_OBJECT,
        .data.object = {.pairs = pairs_cmd, .len = 1, .cap = 1},
    };

    hu_tool_result_t result;
    err = search_tool.vtable->execute(search_tool.ctx, &alloc, &args, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.success);
    HU_ASSERT_STR_CONTAINS(result.output, "shell");

    hu_tool_result_free(&alloc, &result);

    if (search_tool.vtable->deinit)
        search_tool.vtable->deinit(search_tool.ctx, &alloc);
}

static void test_tool_search_case_insensitive(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tools[1];

    tools[0].ctx = NULL;
    tools[0].vtable = &mock_vtable_1;

    hu_tool_t search_tool;
    hu_error_t err = hu_tool_search_create(&alloc, tools, 1, &search_tool);
    HU_ASSERT_EQ(err, HU_OK);

    /* Search for "FILE" (uppercase) — should match file_read (lowercase) */
    hu_json_value_t query_val = {
        .type = HU_JSON_STRING,
        .data.string = {.ptr = "FILE", .len = 4},
    };

    hu_json_pair_t pairs_file[1] = {
        {.key = "query", .key_len = 5, .value = &query_val},
    };

    hu_json_value_t args = {
        .type = HU_JSON_OBJECT,
        .data.object = {.pairs = pairs_file, .len = 1, .cap = 1},
    };

    hu_tool_result_t result;
    err = search_tool.vtable->execute(search_tool.ctx, &alloc, &args, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.success);
    HU_ASSERT_STR_CONTAINS(result.output, "file_read");

    hu_tool_result_free(&alloc, &result);

    if (search_tool.vtable->deinit)
        search_tool.vtable->deinit(search_tool.ctx, &alloc);
}

/* ──────────────────────────────────────────────────────────────────────────
 * Workspace Context Tests
 * ────────────────────────────────────────────────────────────────────────── */

static void test_workspace_context_detect_nodejs(void) {
    hu_allocator_t alloc = hu_system_allocator();

#if HU_IS_TEST
    /* Create temporary directory with package.json */
    char tmpdir[] = "/tmp/hu_test_XXXXXX";
    if (!mkdtemp(tmpdir)) {
        HU_SKIP_IF(1, "Cannot create temp directory");
    }

    char pkg_path[512];
    snprintf(pkg_path, sizeof(pkg_path), "%s/package.json", tmpdir);
    FILE *f = fopen(pkg_path, "w");
    HU_ASSERT_NOT_NULL(f);
    fprintf(f, "{\"name\":\"test-app\",\"version\":\"1.2.3\"}\n");
    fclose(f);

    hu_workspace_context_t ctx;
    hu_error_t err = hu_workspace_context_detect(&alloc, tmpdir, &ctx);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(ctx.project_type, "nodejs");
    HU_ASSERT_STR_EQ(ctx.project_name, "test-app");
    HU_ASSERT_STR_CONTAINS(ctx.summary, "Node.js");
    HU_ASSERT_STR_CONTAINS(ctx.summary, "test-app");

    hu_workspace_context_free(&alloc, &ctx);

    /* Cleanup */
    unlink(pkg_path);
    rmdir(tmpdir);
#endif
}

static void test_workspace_context_detect_python(void) {
    hu_allocator_t alloc = hu_system_allocator();

#if HU_IS_TEST
    /* Create temporary directory with pyproject.toml */
    char tmpdir[] = "/tmp/hu_test_XXXXXX";
    if (!mkdtemp(tmpdir)) {
        HU_SKIP_IF(1, "Cannot create temp directory");
    }

    char proj_path[512];
    snprintf(proj_path, sizeof(proj_path), "%s/pyproject.toml", tmpdir);
    FILE *f = fopen(proj_path, "w");
    HU_ASSERT_NOT_NULL(f);
    fprintf(f, "[project]\nname = \"my-python-app\"\n");
    fclose(f);

    hu_workspace_context_t ctx;
    hu_error_t err = hu_workspace_context_detect(&alloc, tmpdir, &ctx);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(ctx.project_type, "python");
    HU_ASSERT_STR_CONTAINS(ctx.summary, "Python");

    hu_workspace_context_free(&alloc, &ctx);

    /* Cleanup */
    unlink(proj_path);
    rmdir(tmpdir);
#endif
}

static void test_workspace_context_detect_rust(void) {
    hu_allocator_t alloc = hu_system_allocator();

#if HU_IS_TEST
    /* Create temporary directory with Cargo.toml */
    char tmpdir[] = "/tmp/hu_test_XXXXXX";
    if (!mkdtemp(tmpdir)) {
        HU_SKIP_IF(1, "Cannot create temp directory");
    }

    char cargo_path[512];
    snprintf(cargo_path, sizeof(cargo_path), "%s/Cargo.toml", tmpdir);
    FILE *f = fopen(cargo_path, "w");
    HU_ASSERT_NOT_NULL(f);
    fprintf(f, "[package]\nname = \"my-rust-app\"\nversion = \"0.1.0\"\n");
    fclose(f);

    hu_workspace_context_t ctx;
    hu_error_t err = hu_workspace_context_detect(&alloc, tmpdir, &ctx);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(ctx.project_type, "rust");
    HU_ASSERT_STR_CONTAINS(ctx.summary, "Rust");

    hu_workspace_context_free(&alloc, &ctx);

    /* Cleanup */
    unlink(cargo_path);
    rmdir(tmpdir);
#endif
}

static void test_workspace_context_detect_unknown(void) {
    hu_allocator_t alloc = hu_system_allocator();

#if HU_IS_TEST
    /* Create empty temporary directory */
    char tmpdir[] = "/tmp/hu_test_XXXXXX";
    if (!mkdtemp(tmpdir)) {
        HU_SKIP_IF(1, "Cannot create temp directory");
    }

    hu_workspace_context_t ctx;
    hu_error_t err = hu_workspace_context_detect(&alloc, tmpdir, &ctx);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(ctx.project_type, "unknown");
    HU_ASSERT_STR_EQ(ctx.summary, "Unknown project");

    hu_workspace_context_free(&alloc, &ctx);

    /* Cleanup */
    rmdir(tmpdir);
#endif
}

void run_tool_search_tests(void) {
    HU_TEST_SUITE("tool_search");
    HU_RUN_TEST(test_tool_search_create);
    HU_RUN_TEST(test_tool_search_name);
    HU_RUN_TEST(test_tool_search_description);
    HU_RUN_TEST(test_tool_search_parameters_json);
    HU_RUN_TEST(test_tool_search_execute_empty_tools);
    HU_RUN_TEST(test_tool_search_execute_match_by_name);
    HU_RUN_TEST(test_tool_search_execute_match_by_description);
    HU_RUN_TEST(test_tool_search_case_insensitive);

    HU_TEST_SUITE("workspace_context");
    HU_RUN_TEST(test_workspace_context_detect_nodejs);
    HU_RUN_TEST(test_workspace_context_detect_python);
    HU_RUN_TEST(test_workspace_context_detect_rust);
    HU_RUN_TEST(test_workspace_context_detect_unknown);
}
