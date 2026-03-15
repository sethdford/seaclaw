#include "human/agent/mailbox.h"
#include "human/core/allocator.h"
#include "test_framework.h"
#include <string.h>

static void mailbox_message_types(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mailbox_t *mbox = hu_mailbox_create(&alloc, 4);
    HU_ASSERT_NOT_NULL(mbox);
    hu_mailbox_register(mbox, 1);
    hu_mailbox_register(mbox, 2);

    /* All 7 mailbox types can be sent */
    HU_ASSERT_EQ(hu_mailbox_send_ex(mbox, 2, 1, HU_MSG_QUERY, "q", 1, 0,
                                    HU_MSG_PRIO_NORMAL, 0),
                 HU_OK);
    HU_ASSERT_EQ(hu_mailbox_send_ex(mbox, 2, 1, HU_MSG_RESPONSE, "r", 1, 0,
                                    HU_MSG_PRIO_NORMAL, 0),
                 HU_OK);
    HU_ASSERT_EQ(hu_mailbox_send_ex(mbox, 2, 1, HU_MSG_BROADCAST, "b", 1, 0,
                                    HU_MSG_PRIO_NORMAL, 0),
                 HU_OK);
    HU_ASSERT_EQ(hu_mailbox_send_ex(mbox, 2, 1, HU_MSG_PROGRESS, "p", 1, 0,
                                    HU_MSG_PRIO_NORMAL, 0),
                 HU_OK);
    HU_ASSERT_EQ(hu_mailbox_send(mbox, 2, 1, HU_MSG_RESULT, "res", 3, 0), HU_OK);
    HU_ASSERT_EQ(hu_mailbox_send(mbox, 2, 1, HU_MSG_ERROR, "err", 3, 0), HU_OK);
    HU_ASSERT_EQ(hu_mailbox_send(mbox, 2, 1, HU_MSG_CANCEL, "c", 1, 0), HU_OK);

    hu_message_t msg;
    while (hu_mailbox_recv(mbox, 1, &msg) == HU_OK)
        hu_message_free(&alloc, &msg);

    hu_mailbox_destroy(mbox);
}

static void mailbox_priority_ordering(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mailbox_t *mbox = hu_mailbox_create(&alloc, 4);
    HU_ASSERT_NOT_NULL(mbox);
    hu_mailbox_register(mbox, 1);
    hu_mailbox_register(mbox, 2);

    /* Send low, then high, then normal */
    hu_mailbox_send_ex(mbox, 2, 1, HU_MSG_QUERY, "low", 3, 0, HU_MSG_PRIO_LOW, 0);
    hu_mailbox_send_ex(mbox, 2, 1, HU_MSG_QUERY, "high", 4, 0, HU_MSG_PRIO_HIGH, 0);
    hu_mailbox_send_ex(mbox, 2, 1, HU_MSG_QUERY, "norm", 4, 0, HU_MSG_PRIO_NORMAL, 0);

    hu_message_t msg;
    HU_ASSERT_EQ(hu_mailbox_recv(mbox, 1, &msg), HU_OK);
    HU_ASSERT_EQ(msg.priority, HU_MSG_PRIO_HIGH);
    HU_ASSERT_STR_EQ(msg.payload, "high");
    hu_message_free(&alloc, &msg);

    HU_ASSERT_EQ(hu_mailbox_recv(mbox, 1, &msg), HU_OK);
    HU_ASSERT_EQ(msg.priority, HU_MSG_PRIO_NORMAL);
    hu_message_free(&alloc, &msg);

    HU_ASSERT_EQ(hu_mailbox_recv(mbox, 1, &msg), HU_OK);
    HU_ASSERT_EQ(msg.priority, HU_MSG_PRIO_LOW);
    hu_message_free(&alloc, &msg);

    HU_ASSERT_EQ(hu_mailbox_recv(mbox, 1, &msg), HU_ERR_NOT_FOUND);
    hu_mailbox_destroy(mbox);
}

static void mailbox_null_args_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mailbox_t *mbox = hu_mailbox_create(&alloc, 4);
    HU_ASSERT_NOT_NULL(mbox);
    hu_mailbox_register(mbox, 1);

    hu_message_t msg;
    HU_ASSERT_EQ(hu_mailbox_send(NULL, 1, 2, HU_MSG_TASK, "x", 1, 0),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_mailbox_recv(NULL, 1, &msg), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_mailbox_recv(mbox, 1, NULL), HU_ERR_INVALID_ARGUMENT);

    hu_mailbox_destroy(mbox);
}

void run_agent_communication_tests(void) {
    HU_TEST_SUITE("agent_communication");
    HU_RUN_TEST(mailbox_message_types);
    HU_RUN_TEST(mailbox_priority_ordering);
    HU_RUN_TEST(mailbox_null_args_returns_error);
}
