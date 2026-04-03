#include "human/security/delegation.h"
#include "test_framework.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SUITE "delegation"

static hu_allocator_t get_test_alloc(void) {
    return hu_system_allocator();
}

static void test_registry_create_destroy(void) {
    hu_allocator_t alloc = get_test_alloc();
    hu_delegation_registry_t *reg = hu_delegation_registry_create(&alloc);
    HU_ASSERT(reg != NULL);
    HU_ASSERT_EQ(hu_delegation_token_count(reg), 0);
    hu_delegation_registry_destroy(reg);
}

static void test_issue_simple_token(void) {
    hu_allocator_t alloc = get_test_alloc();
    hu_delegation_registry_t *reg = hu_delegation_registry_create(&alloc);
    HU_ASSERT(reg != NULL);

    const char *token_id = hu_delegation_issue(reg, &alloc, "agent_1", "agent_2", 3600, NULL, 0);
    HU_ASSERT(token_id != NULL);
    HU_ASSERT_EQ(hu_delegation_token_count(reg), 1);

    const hu_delegation_token_t *tok = hu_delegation_get_token(reg, token_id);
    HU_ASSERT(tok != NULL);
    HU_ASSERT(strcmp(tok->issuer_agent_id, "agent_1") == 0);
    HU_ASSERT(strcmp(tok->target_agent_id, "agent_2") == 0);
    HU_ASSERT_EQ(tok->caveat_count, 0);
    HU_ASSERT(!tok->revoked);
    HU_ASSERT(strlen(tok->parent_token_id) == 0);

    hu_delegation_registry_destroy(reg);
}

static void test_issue_token_with_caveats(void) {
    hu_allocator_t alloc = get_test_alloc();
    hu_delegation_registry_t *reg = hu_delegation_registry_create(&alloc);

    hu_delegation_caveat_t caveats[2] = {
        {.key = "tool", .key_len = 4, .value = "file_read", .value_len = 9},
        {.key = "path", .key_len = 4, .value = "/workspace/*", .value_len = 12},
    };

    const char *token_id =
        hu_delegation_issue(reg, &alloc, "agent_1", "agent_2", 3600, caveats, 2);
    HU_ASSERT(token_id != NULL);

    const hu_delegation_token_t *tok = hu_delegation_get_token(reg, token_id);
    HU_ASSERT_EQ(tok->caveat_count, 2);
    HU_ASSERT(strcmp(tok->caveats[0].key, "tool") == 0);
    HU_ASSERT(strcmp(tok->caveats[0].value, "file_read") == 0);
    HU_ASSERT(strcmp(tok->caveats[1].key, "path") == 0);
    HU_ASSERT(strcmp(tok->caveats[1].value, "/workspace/*") == 0);

    hu_delegation_registry_destroy(reg);
}

static void test_issue_no_expiry(void) {
    hu_allocator_t alloc = get_test_alloc();
    hu_delegation_registry_t *reg = hu_delegation_registry_create(&alloc);

    const char *token_id = hu_delegation_issue(reg, &alloc, "agent_1", "agent_2", 0, NULL, 0);
    HU_ASSERT(token_id != NULL);

    const hu_delegation_token_t *tok = hu_delegation_get_token(reg, token_id);
    HU_ASSERT_EQ(tok->expires_at, 0);

    hu_delegation_registry_destroy(reg);
}

static void test_verify_simple_allow(void) {
    hu_allocator_t alloc = get_test_alloc();
    hu_delegation_registry_t *reg = hu_delegation_registry_create(&alloc);

    const char *token_id = hu_delegation_issue(reg, &alloc, "agent_1", "agent_2", 0, NULL, 0);
    HU_ASSERT(token_id != NULL);

    hu_error_t err = hu_delegation_verify(reg, token_id, "agent_2", "any_tool", NULL, 0.0);
    HU_ASSERT_EQ(err, HU_OK);

    hu_delegation_registry_destroy(reg);
}

static void test_verify_wrong_agent_deny(void) {
    hu_allocator_t alloc = get_test_alloc();
    hu_delegation_registry_t *reg = hu_delegation_registry_create(&alloc);

    const char *token_id = hu_delegation_issue(reg, &alloc, "agent_1", "agent_2", 0, NULL, 0);
    HU_ASSERT(token_id != NULL);

    hu_error_t err = hu_delegation_verify(reg, token_id, "agent_3", "any_tool", NULL, 0.0);
    HU_ASSERT_NEQ(err, HU_OK);

    hu_delegation_registry_destroy(reg);
}

static void test_revoke_simple(void) {
    hu_allocator_t alloc = get_test_alloc();
    hu_delegation_registry_t *reg = hu_delegation_registry_create(&alloc);

    const char *token_id = hu_delegation_issue(reg, &alloc, "agent_1", "agent_2", 0, NULL, 0);
    HU_ASSERT(token_id != NULL);

    hu_error_t err = hu_delegation_revoke(reg, token_id);
    HU_ASSERT_EQ(err, HU_OK);

    err = hu_delegation_verify(reg, token_id, "agent_2", "tool", NULL, 0.0);
    HU_ASSERT_NEQ(err, HU_OK);

    hu_delegation_registry_destroy(reg);
}

static void test_chain_simple(void) {
    hu_allocator_t alloc = get_test_alloc();
    hu_delegation_registry_t *reg = hu_delegation_registry_create(&alloc);

    const char *token1 =
        hu_delegation_issue(reg, &alloc, "agent_1", "agent_2", 0, NULL, 0);
    const char *token2 =
        hu_delegation_attenuate(reg, &alloc, token1, "agent_2", "agent_3", NULL, 0);
    const char *token3 =
        hu_delegation_attenuate(reg, &alloc, token2, "agent_3", "agent_4", NULL, 0);

    const char **chain = NULL;
    size_t chain_len = 0;
    hu_error_t err = hu_delegation_chain(reg, &alloc, token3, &chain, &chain_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(chain_len, 3);
    HU_ASSERT(strcmp(chain[0], token1) == 0);
    HU_ASSERT(strcmp(chain[1], token2) == 0);
    HU_ASSERT(strcmp(chain[2], token3) == 0);

    alloc.free(alloc.ctx, chain, chain_len * sizeof(char *));
    hu_delegation_registry_destroy(reg);
}

void run_delegation_tests(void) {
    HU_TEST_SUITE("delegation");
    HU_RUN_TEST(test_registry_create_destroy);
    HU_RUN_TEST(test_issue_simple_token);
    HU_RUN_TEST(test_issue_token_with_caveats);
    HU_RUN_TEST(test_issue_no_expiry);
    HU_RUN_TEST(test_verify_simple_allow);
    HU_RUN_TEST(test_verify_wrong_agent_deny);
    HU_RUN_TEST(test_revoke_simple);
    HU_RUN_TEST(test_chain_simple);
}
