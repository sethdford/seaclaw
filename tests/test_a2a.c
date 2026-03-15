#include "test_framework.h"
#include "human/a2a.h"

static void test_a2a_discover(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_a2a_agent_card_t card;
    hu_error_t err = hu_a2a_discover(&alloc, "https://example.com", &card);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT(card.name != NULL);
    HU_ASSERT_STR_EQ(card.name, "test-agent");
    hu_a2a_agent_card_free(&alloc, &card);
}

static void test_a2a_send_task(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_a2a_message_t msg = { .role = "user", .parts = NULL, .parts_count = 0 };
    hu_a2a_task_t task;
    hu_error_t err = hu_a2a_send_task(&alloc, "https://example.com", &msg, &task);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(task.id, "task-001");
    HU_ASSERT_EQ((int)task.state, (int)HU_A2A_TASK_SUBMITTED);
    hu_a2a_task_free(&alloc, &task);
}

static void test_a2a_get_task(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_a2a_task_t task;
    hu_error_t err = hu_a2a_get_task(&alloc, "https://example.com", "task-001", &task);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ((int)task.state, (int)HU_A2A_TASK_COMPLETED);
    hu_a2a_task_free(&alloc, &task);
}

static void test_a2a_cancel(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_error_t err = hu_a2a_cancel_task(&alloc, "https://example.com", "task-001");
    HU_ASSERT_EQ(err, HU_OK);
}

static void test_a2a_server(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_a2a_agent_card_t card = { .name = "test", .description = "d", .url = "u", .version = "1" };
    HU_ASSERT_EQ(hu_a2a_server_init(&alloc, &card), HU_OK);
    char *resp = NULL; size_t rlen = 0;
    HU_ASSERT_EQ(hu_a2a_server_handle_request(&alloc, "GET", NULL, 0, &resp, &rlen), HU_OK);
    HU_ASSERT(resp != NULL);
    alloc.free(alloc.ctx, resp, rlen + 1);
    hu_a2a_server_deinit(&alloc);
}

static void test_a2a_null(void) {
    hu_a2a_agent_card_t card;
    HU_ASSERT_EQ(hu_a2a_discover(NULL, "url", &card), HU_ERR_INVALID_ARGUMENT);
}

void run_a2a_tests(void) {
    HU_TEST_SUITE("A2A Protocol");
    HU_RUN_TEST(test_a2a_discover);
    HU_RUN_TEST(test_a2a_send_task);
    HU_RUN_TEST(test_a2a_get_task);
    HU_RUN_TEST(test_a2a_cancel);
    HU_RUN_TEST(test_a2a_server);
    HU_RUN_TEST(test_a2a_null);
}
