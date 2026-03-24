#include "human/mcp_resources.h"
#include "test_framework.h"
#include <string.h>

static void *test_alloc(void *ctx, size_t size) {
    (void)ctx;
    return malloc(size);
}
static void test_free(void *ctx, void *ptr, size_t size) {
    (void)ctx;
    (void)size;
    free(ptr);
}
static hu_allocator_t test_allocator = {.alloc = test_alloc, .free = test_free, .ctx = NULL};

/* Resource registry tests */

static void test_resource_registry_init(void) {
    hu_mcp_resource_registry_t reg;
    hu_mcp_resource_registry_init(&reg);
    HU_ASSERT_EQ(reg.resource_count, 0);
    HU_ASSERT_EQ(reg.template_count, 0);
}

static void test_resource_register(void) {
    hu_mcp_resource_registry_t reg;
    hu_mcp_resource_registry_init(&reg);

    hu_error_t err = hu_mcp_resource_register(&reg, "file:///project/README.md", "README",
                                              "Project readme", "text/markdown");
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(reg.resource_count, 1);
    HU_ASSERT_STR_EQ(reg.resources[0].uri, "file:///project/README.md");
    HU_ASSERT_STR_EQ(reg.resources[0].name, "README");
    HU_ASSERT_STR_EQ(reg.resources[0].mime_type, "text/markdown");
}

static void test_resource_template_register(void) {
    hu_mcp_resource_registry_t reg;
    hu_mcp_resource_registry_init(&reg);

    hu_error_t err = hu_mcp_resource_template_register(&reg, "file:///{path}", "File",
                                                       "Read a file", "text/plain");
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(reg.template_count, 1);
    HU_ASSERT_STR_EQ(reg.templates[0].uri_template, "file:///{path}");
}

static void test_resource_list_json(void) {
    hu_mcp_resource_registry_t reg;
    hu_mcp_resource_registry_init(&reg);

    hu_mcp_resource_register(&reg, "file:///a.txt", "a", "file a", "text/plain");
    hu_mcp_resource_register(&reg, "db://users", "users", "user table", "application/json");

    char *json = NULL;
    size_t json_len = 0;
    hu_error_t err = hu_mcp_resource_list_json(&test_allocator, &reg, &json, &json_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(json);
    HU_ASSERT_TRUE(json_len > 0);
    HU_ASSERT_STR_CONTAINS(json, "\"resources\"");
    HU_ASSERT_STR_CONTAINS(json, "file:///a.txt");
    HU_ASSERT_STR_CONTAINS(json, "db://users");

    test_allocator.free(NULL, json, json_len + 1);
}

static void test_resource_null_args(void) {
    hu_mcp_resource_registry_init(NULL);
    HU_ASSERT_EQ(hu_mcp_resource_register(NULL, "a", "b", "c", "d"), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_mcp_resource_list_json(NULL, NULL, NULL, NULL), HU_ERR_INVALID_ARGUMENT);
}

/* Prompt registry tests */

static void test_prompt_registry_init(void) {
    hu_mcp_prompt_registry_t reg;
    hu_mcp_prompt_registry_init(&reg);
    HU_ASSERT_EQ(reg.prompt_count, 0);
}

static void test_prompt_register(void) {
    hu_mcp_prompt_registry_t reg;
    hu_mcp_prompt_registry_init(&reg);

    hu_mcp_prompt_t p;
    memset(&p, 0, sizeof(p));
    snprintf(p.name, sizeof(p.name), "summarize");
    snprintf(p.description, sizeof(p.description), "Summarize text");
    snprintf(p.template_text, sizeof(p.template_text), "Summarize the following text:\n\n{{text}}");
    p.template_text_len = strlen(p.template_text);
    p.arguments[0] = (hu_mcp_prompt_arg_t){.required = true};
    snprintf(p.arguments[0].name, sizeof(p.arguments[0].name), "text");
    snprintf(p.arguments[0].description, sizeof(p.arguments[0].description), "Text to summarize");
    p.argument_count = 1;

    hu_error_t err = hu_mcp_prompt_register(&reg, &p);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(reg.prompt_count, 1);
    HU_ASSERT_STR_EQ(reg.prompts[0].name, "summarize");
}

static void test_prompt_render(void) {
    hu_mcp_prompt_t p;
    memset(&p, 0, sizeof(p));
    snprintf(p.template_text, sizeof(p.template_text), "Hello {{name}}, welcome to {{place}}!");
    p.template_text_len = strlen(p.template_text);

    const char *names[] = {"name", "place"};
    const char *values[] = {"Alice", "Wonderland"};
    char *result = NULL;
    size_t result_len = 0;

    hu_error_t err =
        hu_mcp_prompt_render(&test_allocator, &p, names, values, 2, &result, &result_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(result);
    HU_ASSERT_STR_CONTAINS(result, "Hello Alice");
    HU_ASSERT_STR_CONTAINS(result, "welcome to Wonderland");

    test_allocator.free(NULL, result, result_len + 1);
}

static void test_prompt_render_missing_arg(void) {
    hu_mcp_prompt_t p;
    memset(&p, 0, sizeof(p));
    snprintf(p.template_text, sizeof(p.template_text), "Hi {{unknown}}!");
    p.template_text_len = strlen(p.template_text);

    char *result = NULL;
    size_t result_len = 0;
    hu_error_t err = hu_mcp_prompt_render(&test_allocator, &p, NULL, NULL, 0, &result, &result_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_CONTAINS(result, "{{unknown}}");

    test_allocator.free(NULL, result, result_len + 1);
}

static void test_prompt_list_json(void) {
    hu_mcp_prompt_registry_t reg;
    hu_mcp_prompt_registry_init(&reg);

    hu_mcp_prompt_t p;
    memset(&p, 0, sizeof(p));
    snprintf(p.name, sizeof(p.name), "greet");
    snprintf(p.description, sizeof(p.description), "Greet someone");
    p.arguments[0] = (hu_mcp_prompt_arg_t){.required = true};
    snprintf(p.arguments[0].name, sizeof(p.arguments[0].name), "name");
    snprintf(p.arguments[0].description, sizeof(p.arguments[0].description), "Person to greet");
    p.argument_count = 1;
    hu_mcp_prompt_register(&reg, &p);

    char *json = NULL;
    size_t json_len = 0;
    hu_error_t err = hu_mcp_prompt_list_json(&test_allocator, &reg, &json, &json_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(json);
    HU_ASSERT_STR_CONTAINS(json, "\"prompts\"");
    HU_ASSERT_STR_CONTAINS(json, "\"greet\"");
    HU_ASSERT_STR_CONTAINS(json, "\"name\"");

    test_allocator.free(NULL, json, json_len + 1);
}

static void test_prompt_null_args(void) {
    hu_mcp_prompt_registry_init(NULL);
    HU_ASSERT_EQ(hu_mcp_prompt_register(NULL, NULL), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_mcp_prompt_render(NULL, NULL, NULL, NULL, 0, NULL, NULL),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_mcp_prompt_list_json(NULL, NULL, NULL, NULL), HU_ERR_INVALID_ARGUMENT);
}

void run_mcp_resources_tests(void) {
    HU_TEST_SUITE("MCP Resources + Prompts");
    HU_RUN_TEST(test_resource_registry_init);
    HU_RUN_TEST(test_resource_register);
    HU_RUN_TEST(test_resource_template_register);
    HU_RUN_TEST(test_resource_list_json);
    HU_RUN_TEST(test_resource_null_args);
    HU_RUN_TEST(test_prompt_registry_init);
    HU_RUN_TEST(test_prompt_register);
    HU_RUN_TEST(test_prompt_render);
    HU_RUN_TEST(test_prompt_render_missing_arg);
    HU_RUN_TEST(test_prompt_list_json);
    HU_RUN_TEST(test_prompt_null_args);
}
