#include "human/agent/planner.h"
#include "human/provider.h"
#include "test_framework.h"
#include <stdlib.h>
#include <string.h>

static void *pmw_alloc_fn(void *ctx, size_t size) {
    (void)ctx;
    return malloc(size);
}
static void *pmw_realloc_fn(void *ctx, void *ptr, size_t old_size, size_t new_size) {
    (void)ctx;
    (void)old_size;
    return realloc(ptr, new_size);
}
static void pmw_free_fn(void *ctx, void *ptr, size_t size) {
    (void)ctx;
    (void)size;
    free(ptr);
}

static hu_allocator_t pmw_alloc = {
    .alloc = pmw_alloc_fn,
    .realloc = pmw_realloc_fn,
    .free = pmw_free_fn,
    .ctx = NULL,
};

static hu_error_t pmw_mock_chat(void *ctx, hu_allocator_t *alloc, const hu_chat_request_t *req,
                                const char *model, size_t model_len, double temp,
                                hu_chat_response_t *out) {
    (void)ctx;
    (void)alloc;
    (void)req;
    (void)model;
    (void)model_len;
    (void)temp;
    memset(out, 0, sizeof(*out));
    return HU_OK;
}
static const char *pmw_mock_prov_name(void *ctx) {
    (void)ctx;
    return "mock";
}
static const hu_provider_vtable_t pmw_mock_prov_vt = {
    .chat = pmw_mock_chat,
    .get_name = pmw_mock_prov_name,
};

static void planner_plan_needs_mcts_false_when_few_steps(void) {
    const char *json = "{\"steps\":["
                       "{\"tool\":\"a\",\"args\":{}},{\"tool\":\"b\",\"args\":{}},"
                       "{\"tool\":\"c\",\"args\":{}},{\"tool\":\"d\",\"args\":{},\"depends_on\":[0]}]}";
    hu_plan_t *plan = NULL;
    HU_ASSERT_EQ(hu_planner_create_plan(&pmw_alloc, json, strlen(json), &plan), HU_OK);
    HU_ASSERT_NOT_NULL(plan);
    HU_ASSERT_FALSE(hu_planner_plan_needs_mcts(plan));
    hu_plan_free(&pmw_alloc, plan);
}

static void planner_plan_needs_mcts_false_when_no_dependencies(void) {
    const char *json = "{\"steps\":["
                       "{\"tool\":\"a\",\"args\":{}},{\"tool\":\"b\",\"args\":{}},"
                       "{\"tool\":\"c\",\"args\":{}},{\"tool\":\"d\",\"args\":{}},"
                       "{\"tool\":\"e\",\"args\":{}}]}";
    hu_plan_t *plan = NULL;
    HU_ASSERT_EQ(hu_planner_create_plan(&pmw_alloc, json, strlen(json), &plan), HU_OK);
    HU_ASSERT_FALSE(hu_planner_plan_needs_mcts(plan));
    hu_plan_free(&pmw_alloc, plan);
}

static void planner_plan_needs_mcts_true_with_deps(void) {
    const char *json = "{\"steps\":["
                       "{\"tool\":\"a\",\"args\":{}},{\"tool\":\"b\",\"args\":{}},"
                       "{\"tool\":\"c\",\"args\":{}},{\"tool\":\"d\",\"args\":{}},"
                       "{\"tool\":\"e\",\"args\":{},\"depends_on\":[3]}]}";
    hu_plan_t *plan = NULL;
    HU_ASSERT_EQ(hu_planner_create_plan(&pmw_alloc, json, strlen(json), &plan), HU_OK);
    HU_ASSERT_TRUE(hu_planner_plan_needs_mcts(plan));
    HU_ASSERT_EQ(plan->steps[4].depends_count, (size_t)1);
    HU_ASSERT_EQ(plan->steps[4].depends_on[0], 3);
    hu_plan_free(&pmw_alloc, plan);
}

static void planner_plan_mcts_returns_stub_plan_in_tests(void) {
    hu_provider_t prov = {.ctx = NULL, .vtable = &pmw_mock_prov_vt};
    hu_plan_t *plan = NULL;
    HU_ASSERT_EQ(hu_planner_plan_mcts(&pmw_alloc, &prov, "m", 1, "do something", 12, NULL, 0, &plan),
                 HU_OK);
    HU_ASSERT_NOT_NULL(plan);
    HU_ASSERT_TRUE(plan->steps_count > 0);
    hu_plan_free(&pmw_alloc, plan);
}

void run_planner_mcts_wiring_tests(void) {
    HU_TEST_SUITE("Planner");
    HU_RUN_TEST(planner_plan_needs_mcts_false_when_few_steps);
    HU_RUN_TEST(planner_plan_needs_mcts_false_when_no_dependencies);
    HU_RUN_TEST(planner_plan_needs_mcts_true_with_deps);
    HU_RUN_TEST(planner_plan_mcts_returns_stub_plan_in_tests);
}
