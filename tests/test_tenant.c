#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/gateway/tenant.h"
#include "test_framework.h"
#include <stdio.h>
#include <string.h>

static sc_tenant_t make_tenant(const char *id, const char *name, sc_tenant_role_t role,
                               uint32_t rpm, uint64_t quota) {
    sc_tenant_t t;
    memset(&t, 0, sizeof(t));
    snprintf(t.user_id, sizeof(t.user_id), "%s", id);
    snprintf(t.display_name, sizeof(t.display_name), "%s", name);
    t.role = role;
    t.rate_limit_rpm = rpm;
    t.usage_quota_tokens = quota;
    return t;
}

static void test_tenant_store_init_destroy(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tenant_store_t *store = NULL;
    sc_error_t err = sc_tenant_store_init(&alloc, &store);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(store);
    sc_tenant_store_destroy(store);
}

static void test_tenant_crud(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tenant_store_t *store = NULL;
    sc_tenant_store_init(&alloc, &store);
    sc_tenant_t t = make_tenant("user-1", "Alice", SC_TENANT_ROLE_ADMIN, 60, 100000);
    SC_ASSERT_EQ(sc_tenant_create(store, &t), SC_OK);
    sc_tenant_t out = {0};
    SC_ASSERT_EQ(sc_tenant_get(store, "user-1", &out), SC_OK);
    SC_ASSERT_STR_EQ(out.display_name, "Alice");
    SC_ASSERT_EQ((int)out.role, (int)SC_TENANT_ROLE_ADMIN);
    snprintf(t.display_name, sizeof(t.display_name), "Alice Updated");
    SC_ASSERT_EQ(sc_tenant_update(store, &t), SC_OK);
    SC_ASSERT_EQ(sc_tenant_get(store, "user-1", &out), SC_OK);
    SC_ASSERT_STR_EQ(out.display_name, "Alice Updated");
    SC_ASSERT_EQ(sc_tenant_delete(store, "user-1"), SC_OK);
    SC_ASSERT_NEQ(sc_tenant_get(store, "user-1", &out), SC_OK);
    sc_tenant_store_destroy(store);
}

static void test_tenant_duplicate_rejected(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tenant_store_t *store = NULL;
    sc_tenant_store_init(&alloc, &store);
    sc_tenant_t t = make_tenant("dup", "Bob", SC_TENANT_ROLE_USER, 0, 0);
    SC_ASSERT_EQ(sc_tenant_create(store, &t), SC_OK);
    SC_ASSERT_NEQ(sc_tenant_create(store, &t), SC_OK);
    sc_tenant_store_destroy(store);
}

static void test_tenant_list(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tenant_store_t *store = NULL;
    sc_tenant_store_init(&alloc, &store);
    sc_tenant_t ta = make_tenant("a", "A", SC_TENANT_ROLE_USER, 0, 0);
    sc_tenant_t tb = make_tenant("b", "B", SC_TENANT_ROLE_USER, 0, 0);
    SC_ASSERT_EQ(sc_tenant_create(store, &ta), SC_OK);
    SC_ASSERT_EQ(sc_tenant_create(store, &tb), SC_OK);
    sc_tenant_t out[10];
    size_t count = 0;
    SC_ASSERT_EQ(sc_tenant_list(store, out, 10, &count), SC_OK);
    SC_ASSERT_EQ(count, 2u);
    sc_tenant_store_destroy(store);
}

static void test_tenant_quota_enforcement(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tenant_store_t *store = NULL;
    sc_tenant_store_init(&alloc, &store);
    sc_tenant_t t = make_tenant("quota", "Q", SC_TENANT_ROLE_USER, 0, 1000);
    sc_tenant_create(store, &t);
    SC_ASSERT_TRUE(sc_tenant_check_quota(store, "quota"));
    sc_tenant_increment_usage(store, "quota", 999);
    SC_ASSERT_TRUE(sc_tenant_check_quota(store, "quota"));
    sc_tenant_increment_usage(store, "quota", 2);
    SC_ASSERT_FALSE(sc_tenant_check_quota(store, "quota"));
    sc_tenant_store_destroy(store);
}

static void test_tenant_rate_limit(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tenant_store_t *store = NULL;
    sc_tenant_store_init(&alloc, &store);
    sc_tenant_t t = make_tenant("rl", "RL", SC_TENANT_ROLE_USER, 3, 0);
    sc_tenant_create(store, &t);
    SC_ASSERT_TRUE(sc_tenant_check_rate_limit(store, "rl"));
    SC_ASSERT_TRUE(sc_tenant_check_rate_limit(store, "rl"));
    SC_ASSERT_TRUE(sc_tenant_check_rate_limit(store, "rl"));
    SC_ASSERT_FALSE(sc_tenant_check_rate_limit(store, "rl"));
    sc_tenant_store_destroy(store);
}

static void test_tenant_unlimited_quota(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tenant_store_t *store = NULL;
    sc_tenant_store_init(&alloc, &store);
    sc_tenant_t t = make_tenant("unl", "U", SC_TENANT_ROLE_ADMIN, 0, 0);
    sc_tenant_create(store, &t);
    SC_ASSERT_TRUE(sc_tenant_check_quota(store, "unl"));
    SC_ASSERT_TRUE(sc_tenant_check_rate_limit(store, "unl"));
    sc_tenant_increment_usage(store, "unl", 999999);
    SC_ASSERT_TRUE(sc_tenant_check_quota(store, "unl"));
    sc_tenant_store_destroy(store);
}

static void test_tenant_readonly_role(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tenant_store_t *store = NULL;
    sc_tenant_store_init(&alloc, &store);
    sc_tenant_t t = make_tenant("ro", "RO", SC_TENANT_ROLE_READONLY, 10, 5000);
    sc_tenant_create(store, &t);
    sc_tenant_t out = {0};
    sc_tenant_get(store, "ro", &out);
    SC_ASSERT_EQ((int)out.role, (int)SC_TENANT_ROLE_READONLY);
    sc_tenant_store_destroy(store);
}

static void test_tenant_not_found(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tenant_store_t *store = NULL;
    sc_tenant_store_init(&alloc, &store);
    sc_tenant_t out = {0};
    SC_ASSERT_NEQ(sc_tenant_get(store, "nonexistent", &out), SC_OK);
    SC_ASSERT_FALSE(sc_tenant_check_quota(store, "nonexistent"));
    SC_ASSERT_FALSE(sc_tenant_check_rate_limit(store, "nonexistent"));
    sc_tenant_store_destroy(store);
}

static void test_tenant_null_args(void) {
    SC_ASSERT_NEQ(sc_tenant_store_init(NULL, NULL), SC_OK);
    SC_ASSERT_NEQ(sc_tenant_create(NULL, NULL), SC_OK);
    SC_ASSERT_NEQ(sc_tenant_get(NULL, NULL, NULL), SC_OK);
}

void run_tenant_tests(void) {
    SC_TEST_SUITE("tenant");
    SC_RUN_TEST(test_tenant_store_init_destroy);
    SC_RUN_TEST(test_tenant_crud);
    SC_RUN_TEST(test_tenant_duplicate_rejected);
    SC_RUN_TEST(test_tenant_list);
    SC_RUN_TEST(test_tenant_quota_enforcement);
    SC_RUN_TEST(test_tenant_rate_limit);
    SC_RUN_TEST(test_tenant_unlimited_quota);
    SC_RUN_TEST(test_tenant_readonly_role);
    SC_RUN_TEST(test_tenant_not_found);
    SC_RUN_TEST(test_tenant_null_args);
}
