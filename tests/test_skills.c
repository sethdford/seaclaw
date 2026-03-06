/* Skills and skill registry tests. */
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/skill_registry.h"
#include "seaclaw/skills.h"
#include "test_framework.h"
#include <string.h>

static void test_skills_list_delegates_to_skillforge(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_skill_t *skills = NULL;
    size_t count = 99;
    sc_error_t err = sc_skills_list(&alloc, "/tmp", &skills, &count);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(skills);
    SC_ASSERT_EQ(count, 3u);
    SC_ASSERT_STR_EQ(skills[0].name, "test-skill");
    sc_skills_free(&alloc, skills, count);
}

static void test_skills_list_null_workspace_uses_dot(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_skill_t *skills = NULL;
    size_t count = 0;
    sc_error_t err = sc_skills_list(&alloc, NULL, &skills, &count);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(skills);
    SC_ASSERT_EQ(count, 3u);
    sc_skills_free(&alloc, skills, count);
}

static void test_skills_free_null_safe(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_skills_free(&alloc, NULL, 0);
}

static void test_skills_free_with_null_skills(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_skills_free(&alloc, NULL, 5);
}

static void test_skills_list_resets_output(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_skill_t *skills = (sc_skill_t *)0xdeadbeef;
    size_t count = 42;
    sc_error_t err = sc_skills_list(&alloc, ".", &skills, &count);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(skills);
    SC_ASSERT_EQ(count, 3u);
    sc_skills_free(&alloc, skills, count);
}

static void test_skills_list_null_out_returns_error(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_skill_t *skills = NULL;
    size_t count = 0;
    sc_error_t err = sc_skills_list(&alloc, "/tmp", NULL, &count);
    SC_ASSERT_NEQ(err, SC_OK);
    err = sc_skills_list(&alloc, "/tmp", &skills, NULL);
    SC_ASSERT_NEQ(err, SC_OK);
}

static void test_skills_list_empty_workspace_path(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_skill_t *skills = NULL;
    size_t count = 0;
    sc_error_t err = sc_skills_list(&alloc, "", &skills, &count);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(skills);
    SC_ASSERT_TRUE(count >= 1);
    sc_skills_free(&alloc, skills, count);
}

static void test_skills_list_dot_workspace(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_skill_t *skills = NULL;
    size_t count = 0;
    sc_error_t err = sc_skills_list(&alloc, ".", &skills, &count);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(skills);
    SC_ASSERT_EQ(count, 3u);
    sc_skills_free(&alloc, skills, count);
}

/* SC_IS_TEST: skill registry returns mock data without network */
static void test_skill_registry_search_mock(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_skill_registry_entry_t *entries = NULL;
    size_t count = 0;
    sc_error_t err = sc_skill_registry_search(&alloc, "code", &entries, &count);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(entries);
    SC_ASSERT_TRUE(count >= 1);
    SC_ASSERT_STR_EQ(entries[0].name, "code-review");
    sc_skill_registry_entries_free(&alloc, entries, count);
}

static void test_skill_registry_install_mock(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_error_t err = sc_skill_registry_install(&alloc, "code-review");
    SC_ASSERT_EQ(err, SC_ERR_NOT_SUPPORTED);
}

static void test_skill_registry_uninstall_mock(void) {
    sc_error_t err = sc_skill_registry_uninstall("nonexistent");
    SC_ASSERT_EQ(err, SC_ERR_NOT_SUPPORTED);
}

static void test_skill_registry_get_installed_dir(void) {
    char buf[256];
    size_t n = sc_skill_registry_get_installed_dir(buf, sizeof(buf));
    SC_ASSERT_TRUE(n > 0 || (n == 0 && !getenv("HOME")));
    if (n >= 6)
        SC_ASSERT_TRUE(strstr(buf, "skills") != NULL);
}

static void test_skill_registry_publish_mock(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_error_t err = sc_skill_registry_publish(&alloc, "/tmp/skill");
    SC_ASSERT_EQ(err, SC_ERR_NOT_SUPPORTED);
}

static void test_skill_registry_publish_null_dir(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_error_t err = sc_skill_registry_publish(&alloc, NULL);
    SC_ASSERT(err != SC_OK);
}

static void test_skill_registry_search_empty_query(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_skill_registry_entry_t *entries = NULL;
    size_t count = 0;
    sc_error_t err = sc_skill_registry_search(&alloc, "", &entries, &count);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(entries);
    SC_ASSERT_TRUE(count >= 1);
    sc_skill_registry_entries_free(&alloc, entries, count);
}

static void test_skill_registry_search_null_query(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_skill_registry_entry_t *entries = NULL;
    size_t count = 0;
    sc_error_t err = sc_skill_registry_search(&alloc, NULL, &entries, &count);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(entries);
    SC_ASSERT_TRUE(count >= 1);
    sc_skill_registry_entries_free(&alloc, entries, count);
}

static void test_skill_registry_install_null_name(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_error_t err = sc_skill_registry_install(&alloc, NULL);
    SC_ASSERT_EQ(err, SC_ERR_NOT_SUPPORTED);
}

static void test_skill_registry_install_empty_name(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_error_t err = sc_skill_registry_install(&alloc, "");
    SC_ASSERT_EQ(err, SC_ERR_NOT_SUPPORTED);
}

static void test_skill_registry_uninstall_null_name(void) {
    sc_error_t err = sc_skill_registry_uninstall(NULL);
    SC_ASSERT_EQ(err, SC_ERR_NOT_SUPPORTED);
}

static void test_skill_registry_update_null_alloc(void) {
    sc_error_t err = sc_skill_registry_update(NULL);
    SC_ASSERT_EQ(err, SC_ERR_NOT_SUPPORTED);
}

static void test_skill_registry_get_installed_dir_writes_path(void) {
    char buf[512];
    size_t n = sc_skill_registry_get_installed_dir(buf, sizeof(buf));
    if (getenv("HOME")) {
        SC_ASSERT_TRUE(n > 0);
        SC_ASSERT_TRUE(strstr(buf, ".seaclaw") != NULL || strstr(buf, "skills") != NULL);
    }
}

static void test_skill_registry_get_installed_dir_small_buffer(void) {
    char buf[4];
    size_t n = sc_skill_registry_get_installed_dir(buf, 4);
    SC_ASSERT_TRUE(n == 0 || n < 4);
}

static void test_skill_registry_search_mock_returns_entries(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_skill_registry_entry_t *entries = NULL;
    size_t count = 0;
    sc_error_t err = sc_skill_registry_search(&alloc, "code", &entries, &count);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(entries);
    SC_ASSERT_EQ(count, 2u);
    SC_ASSERT_STR_EQ(entries[0].name, "code-review");
    SC_ASSERT_STR_EQ(entries[1].name, "email-digest");
    sc_skill_registry_entries_free(&alloc, entries, count);
}

static void test_skill_registry_entries_free_null_safe(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_skill_registry_entries_free(&alloc, NULL, 0);
    sc_skill_registry_entries_free(&alloc, NULL, 5);
}

void run_skills_tests(void) {
    SC_TEST_SUITE("Skills");
    SC_RUN_TEST(test_skills_list_delegates_to_skillforge);
    SC_RUN_TEST(test_skills_list_null_workspace_uses_dot);
    SC_RUN_TEST(test_skills_free_null_safe);
    SC_RUN_TEST(test_skills_free_with_null_skills);
    SC_RUN_TEST(test_skills_list_resets_output);
    SC_RUN_TEST(test_skills_list_null_out_returns_error);
    SC_RUN_TEST(test_skills_list_empty_workspace_path);
    SC_RUN_TEST(test_skills_list_dot_workspace);
    SC_RUN_TEST(test_skill_registry_search_mock);
    SC_RUN_TEST(test_skill_registry_install_mock);
    SC_RUN_TEST(test_skill_registry_uninstall_mock);
    SC_RUN_TEST(test_skill_registry_get_installed_dir);
    SC_RUN_TEST(test_skill_registry_publish_mock);
    SC_RUN_TEST(test_skill_registry_publish_null_dir);
    SC_RUN_TEST(test_skill_registry_search_empty_query);
    SC_RUN_TEST(test_skill_registry_search_null_query);
    SC_RUN_TEST(test_skill_registry_install_null_name);
    SC_RUN_TEST(test_skill_registry_install_empty_name);
    SC_RUN_TEST(test_skill_registry_uninstall_null_name);
    SC_RUN_TEST(test_skill_registry_update_null_alloc);
    SC_RUN_TEST(test_skill_registry_get_installed_dir_writes_path);
    SC_RUN_TEST(test_skill_registry_get_installed_dir_small_buffer);
    SC_RUN_TEST(test_skill_registry_search_mock_returns_entries);
    SC_RUN_TEST(test_skill_registry_entries_free_null_safe);
}
