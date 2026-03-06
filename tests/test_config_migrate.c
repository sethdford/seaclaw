#include "seaclaw/config.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/core/json.h"
#include "test_framework.h"
#include <string.h>

static void test_migrate_missing_version_defaults_to_v1(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_json_value_t *root = sc_json_object_new(&alloc);
    sc_error_t err = sc_config_migrate(&alloc, root);
    SC_ASSERT_EQ(err, SC_OK);
    double ver = sc_json_get_number(root, "config_version", 0);
    SC_ASSERT_EQ((int)ver, SC_CONFIG_VERSION_CURRENT);
    sc_json_free(&alloc, root);
}

static void test_migrate_current_version_noop(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_json_value_t *root = sc_json_object_new(&alloc);
    sc_json_object_set(&alloc, root, "config_version",
                       sc_json_number_new(&alloc, (double)SC_CONFIG_VERSION_CURRENT));
    sc_error_t err = sc_config_migrate(&alloc, root);
    SC_ASSERT_EQ(err, SC_OK);
    sc_json_free(&alloc, root);
}

static void test_migrate_future_version_rejected(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_json_value_t *root = sc_json_object_new(&alloc);
    sc_json_object_set(&alloc, root, "config_version", sc_json_number_new(&alloc, 999.0));
    sc_error_t err = sc_config_migrate(&alloc, root);
    SC_ASSERT_NEQ(err, SC_OK);
    sc_json_free(&alloc, root);
}

static void test_migrate_v1_memory_backend_moved(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_json_value_t *root = sc_json_object_new(&alloc);
    sc_json_object_set(&alloc, root, "memory_backend", sc_json_string_new(&alloc, "sqlite", 6));
    sc_error_t err = sc_config_migrate(&alloc, root);
    SC_ASSERT_EQ(err, SC_OK);
    const sc_json_value_t *mem = sc_json_object_get(root, "memory");
    SC_ASSERT_TRUE(mem != NULL);
    if (mem) {
        const char *be = sc_json_get_string(mem, "backend");
        SC_ASSERT_TRUE(be != NULL);
        if (be)
            SC_ASSERT_STR_EQ(be, "sqlite");
    }
    sc_json_free(&alloc, root);
}

void run_config_migrate_tests(void) {
    SC_RUN_TEST(test_migrate_missing_version_defaults_to_v1);
    SC_RUN_TEST(test_migrate_current_version_noop);
    SC_RUN_TEST(test_migrate_future_version_rejected);
    SC_RUN_TEST(test_migrate_v1_memory_backend_moved);
}
