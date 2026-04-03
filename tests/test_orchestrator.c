#include "test_framework.h"
#include "human/agent/mailbox.h"
#include "human/agent/orchestrator.h"
#include "human/agent/orchestrator_llm.h"
#include "human/core/allocator.h"
#include <string.h>

static void test_orchestrator_decompose_goal_returns_research_synthesize(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_decomposition_t result;
    memset(&result, 0, sizeof(result));

    HU_ASSERT_EQ(hu_orchestrator_decompose_goal(&alloc, NULL, "gpt-4", 5, "complex goal", 12, NULL, 0,
                                                &result),
                 HU_OK);
    HU_ASSERT_EQ(result.task_count, 2u);
    HU_ASSERT_STR_EQ(result.tasks[0].description, "research");
    HU_ASSERT_STR_EQ(result.tasks[1].description, "synthesize");
    hu_decomposition_free(&alloc, &result);
}

static void test_orchestrator_merge_consensus_picks_longest_when_similar(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_orchestrator_t orch;
    HU_ASSERT_EQ(hu_orchestrator_create(&alloc, &orch), HU_OK);

    const char *tasks[] = {"a", "b", "c"};
    size_t lens[] = {1, 1, 1};
    hu_orchestrator_propose_split(&orch, "g", 1, tasks, lens, 3);
    hu_orchestrator_assign_task(&orch, 1, "x", 1);
    hu_orchestrator_assign_task(&orch, 2, "y", 1);
    hu_orchestrator_assign_task(&orch, 3, "z", 1);

    hu_orchestrator_complete_task(&orch, 1, "the quick brown fox", 19);
    hu_orchestrator_complete_task(&orch, 2, "quick brown fox jumps", 21);
    hu_orchestrator_complete_task(&orch, 3, "the quick brown fox jumps over", 30);

    char *merged = NULL;
    size_t merged_len = 0;
    HU_ASSERT_EQ(hu_orchestrator_merge_results_consensus(&orch, &alloc, &merged, &merged_len), HU_OK);
    HU_ASSERT_NOT_NULL(merged);
    HU_ASSERT_TRUE(merged_len > 0);
    HU_ASSERT_STR_EQ(merged, "the quick brown fox jumps over");
    alloc.free(alloc.ctx, merged, merged_len + 1);
    hu_orchestrator_deinit(&orch);
}

static void test_orchestrator_merge_consensus_divergent_keeps_all(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_orchestrator_t orch;
    HU_ASSERT_EQ(hu_orchestrator_create(&alloc, &orch), HU_OK);

    const char *tasks[] = {"a", "b"};
    size_t lens[] = {1, 1};
    hu_orchestrator_propose_split(&orch, "g", 1, tasks, lens, 2);
    hu_orchestrator_assign_task(&orch, 1, "x", 1);
    hu_orchestrator_assign_task(&orch, 2, "y", 1);
    hu_orchestrator_complete_task(&orch, 1, "apple banana cherry", 19);
    hu_orchestrator_complete_task(&orch, 2, "xray zebra quantum", 18);

    char *merged = NULL;
    size_t merged_len = 0;
    HU_ASSERT_EQ(hu_orchestrator_merge_results_consensus(&orch, &alloc, &merged, &merged_len), HU_OK);
    HU_ASSERT_NOT_NULL(merged);
    HU_ASSERT_TRUE(merged_len > 40);
    HU_ASSERT_TRUE(strstr(merged, "Multiple perspectives") != NULL);
    HU_ASSERT_TRUE(strstr(merged, "apple") != NULL);
    HU_ASSERT_TRUE(strstr(merged, "zebra") != NULL);
    alloc.free(alloc.ctx, merged, merged_len + 1);
    hu_orchestrator_deinit(&orch);
}

static void test_orchestrator_merge_consensus_single_task(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_orchestrator_t orch;
    hu_orchestrator_create(&alloc, &orch);
    const char *tasks[] = {"only"};
    size_t lens[] = {4};
    hu_orchestrator_propose_split(&orch, "g", 1, tasks, lens, 1);
    hu_orchestrator_assign_task(&orch, 1, "a", 1);
    hu_orchestrator_complete_task(&orch, 1, "solo result", 11);

    char *merged = NULL;
    size_t merged_len = 0;
    HU_ASSERT_EQ(hu_orchestrator_merge_results_consensus(&orch, &alloc, &merged, &merged_len), HU_OK);
    HU_ASSERT_NOT_NULL(merged);
    HU_ASSERT_STR_EQ(merged, "solo result");
    alloc.free(alloc.ctx, merged, merged_len + 1);
    hu_orchestrator_deinit(&orch);
}

static void test_orchestrator_merge_consensus_fresh_no_tasks_returns_empty(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_orchestrator_t orch;
    HU_ASSERT_EQ(hu_orchestrator_create(&alloc, &orch), HU_OK);

    char *merged = NULL;
    size_t merged_len = 1;
    HU_ASSERT_EQ(hu_orchestrator_merge_results_consensus(&orch, &alloc, &merged, &merged_len), HU_OK);
    HU_ASSERT_NULL(merged);
    HU_ASSERT_EQ(merged_len, 0u);
    hu_orchestrator_deinit(&orch);
}

static void test_orchestrator_merge_consensus_all_failed_returns_empty(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_orchestrator_t orch;
    HU_ASSERT_EQ(hu_orchestrator_create(&alloc, &orch), HU_OK);

    const char *tasks[] = {"a", "b"};
    size_t lens[] = {1, 1};
    HU_ASSERT_EQ(hu_orchestrator_propose_split(&orch, "g", 1, tasks, lens, 2), HU_OK);
    HU_ASSERT_EQ(hu_orchestrator_assign_task(&orch, 1, "x", 1), HU_OK);
    HU_ASSERT_EQ(hu_orchestrator_assign_task(&orch, 2, "y", 1), HU_OK);
    HU_ASSERT_EQ(hu_orchestrator_fail_task(&orch, 1, "e1", 2), HU_OK);
    HU_ASSERT_EQ(hu_orchestrator_fail_task(&orch, 2, "e2", 2), HU_OK);

    char *merged = NULL;
    size_t merged_len = 1;
    HU_ASSERT_EQ(hu_orchestrator_merge_results_consensus(&orch, &alloc, &merged, &merged_len), HU_OK);
    HU_ASSERT_NULL(merged);
    HU_ASSERT_EQ(merged_len, 0u);
    hu_orchestrator_deinit(&orch);
}

static void test_orchestrator_merge_results_empty_completed_returns_ok(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_orchestrator_t orch;
    HU_ASSERT_EQ(hu_orchestrator_create(&alloc, &orch), HU_OK);

    char *merged = NULL;
    size_t merged_len = 1;
    HU_ASSERT_EQ(hu_orchestrator_merge_results(&orch, &alloc, &merged, &merged_len), HU_OK);
    HU_ASSERT_NULL(merged);
    HU_ASSERT_EQ(merged_len, 0u);
    hu_orchestrator_deinit(&orch);
}

/* ── Multi-agent coordination: two agents exchange tasks via shared mailbox ── */

static void test_multi_agent_mailbox_task_delegation_and_reply(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mailbox_t *mbox = hu_mailbox_create(&alloc, 4);
    HU_ASSERT_NOT_NULL(mbox);

    uint64_t coordinator_id = 100;
    uint64_t worker_id = 200;
    HU_ASSERT_EQ(hu_mailbox_register(mbox, coordinator_id), HU_OK);
    HU_ASSERT_EQ(hu_mailbox_register(mbox, worker_id), HU_OK);

    /* Coordinator assigns a task to the worker */
    const char *task_payload = "{\"task\":\"summarize\",\"doc\":\"report.pdf\"}";
    uint64_t correlation = 42;
    HU_ASSERT_EQ(hu_mailbox_send(mbox, coordinator_id, worker_id, HU_MSG_TASK, task_payload,
                                  strlen(task_payload), correlation),
                 HU_OK);

    /* Worker receives the task */
    HU_ASSERT_EQ(hu_mailbox_pending_count(mbox, worker_id), 1u);
    hu_message_t msg;
    memset(&msg, 0, sizeof(msg));
    HU_ASSERT_EQ(hu_mailbox_recv(mbox, worker_id, &msg), HU_OK);
    HU_ASSERT_EQ(msg.type, HU_MSG_TASK);
    HU_ASSERT_EQ(msg.from_agent, coordinator_id);
    HU_ASSERT_EQ(msg.to_agent, worker_id);
    HU_ASSERT_EQ(msg.correlation_id, correlation);
    HU_ASSERT_NOT_NULL(msg.payload);
    HU_ASSERT_NOT_NULL(strstr(msg.payload, "summarize"));
    hu_message_free(&alloc, &msg);

    /* Worker sends result back to coordinator */
    const char *result_payload = "{\"summary\":\"The report covers Q3 revenue.\"}";
    HU_ASSERT_EQ(hu_mailbox_send(mbox, worker_id, coordinator_id, HU_MSG_RESULT, result_payload,
                                  strlen(result_payload), correlation),
                 HU_OK);

    /* Coordinator receives the result */
    memset(&msg, 0, sizeof(msg));
    HU_ASSERT_EQ(hu_mailbox_recv(mbox, coordinator_id, &msg), HU_OK);
    HU_ASSERT_EQ(msg.type, HU_MSG_RESULT);
    HU_ASSERT_EQ(msg.from_agent, worker_id);
    HU_ASSERT_EQ(msg.correlation_id, correlation);
    HU_ASSERT_NOT_NULL(strstr(msg.payload, "Q3 revenue"));
    hu_message_free(&alloc, &msg);

    /* Both inboxes are now empty */
    HU_ASSERT_EQ(hu_mailbox_pending_count(mbox, coordinator_id), 0u);
    HU_ASSERT_EQ(hu_mailbox_pending_count(mbox, worker_id), 0u);

    hu_mailbox_destroy(mbox);
}

static void test_multi_agent_orchestrator_decompose_assign_merge(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_orchestrator_t orch;
    HU_ASSERT_EQ(hu_orchestrator_create(&alloc, &orch), HU_OK);

    /* Decompose a goal into subtasks */
    const char *tasks[] = {"research market trends", "draft executive summary"};
    size_t lens[] = {22, 23};
    hu_orchestrator_propose_split(&orch, "quarterly report", 16, tasks, lens, 2);
    HU_ASSERT_EQ(orch.task_count, 2u);

    /* Assign to different agents */
    hu_orchestrator_assign_task(&orch, 1, "researcher", 10);
    hu_orchestrator_assign_task(&orch, 2, "writer", 6);

    /* Verify assignments */
    HU_ASSERT_STR_EQ(orch.tasks[0].assigned_agent, "researcher");
    HU_ASSERT_STR_EQ(orch.tasks[1].assigned_agent, "writer");

    /* Each agent completes their task */
    hu_orchestrator_complete_task(&orch, 1, "Market grew 15% YoY in Q3", 25);
    hu_orchestrator_complete_task(&orch, 2, "Revenue exceeded expectations", 29);

    HU_ASSERT_TRUE(hu_orchestrator_all_complete(&orch));

    /* Merge results */
    char *merged = NULL;
    size_t merged_len = 0;
    HU_ASSERT_EQ(hu_orchestrator_merge_results(&orch, &alloc, &merged, &merged_len), HU_OK);
    HU_ASSERT_NOT_NULL(merged);
    HU_ASSERT_TRUE(merged_len > 0);
    HU_ASSERT_NOT_NULL(strstr(merged, "Market grew"));
    HU_ASSERT_NOT_NULL(strstr(merged, "Revenue exceeded"));
    alloc.free(alloc.ctx, merged, merged_len + 1);
    hu_orchestrator_deinit(&orch);
}

static void test_multi_agent_broadcast_reaches_all_workers(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mailbox_t *mbox = hu_mailbox_create(&alloc, 4);
    HU_ASSERT_NOT_NULL(mbox);

    uint64_t coordinator = 1;
    uint64_t worker_a = 2;
    uint64_t worker_b = 3;
    HU_ASSERT_EQ(hu_mailbox_register(mbox, coordinator), HU_OK);
    HU_ASSERT_EQ(hu_mailbox_register(mbox, worker_a), HU_OK);
    HU_ASSERT_EQ(hu_mailbox_register(mbox, worker_b), HU_OK);

    HU_ASSERT_EQ(hu_mailbox_broadcast(mbox, coordinator, HU_MSG_TASK, "cancel all", 10), HU_OK);

    HU_ASSERT_EQ(hu_mailbox_pending_count(mbox, worker_a), 1u);
    HU_ASSERT_EQ(hu_mailbox_pending_count(mbox, worker_b), 1u);
    HU_ASSERT_EQ(hu_mailbox_pending_count(mbox, coordinator), 0u);

    hu_message_t msg;
    HU_ASSERT_EQ(hu_mailbox_recv(mbox, worker_a, &msg), HU_OK);
    HU_ASSERT_NOT_NULL(strstr(msg.payload, "cancel all"));
    hu_message_free(&alloc, &msg);
    HU_ASSERT_EQ(hu_mailbox_recv(mbox, worker_b, &msg), HU_OK);
    HU_ASSERT_NOT_NULL(strstr(msg.payload, "cancel all"));
    hu_message_free(&alloc, &msg);

    hu_mailbox_destroy(mbox);
}

void run_orchestrator_tests(void) {
    HU_TEST_SUITE("Orchestrator");

    HU_RUN_TEST(test_orchestrator_decompose_goal_returns_research_synthesize);
    HU_RUN_TEST(test_orchestrator_merge_consensus_picks_longest_when_similar);
    HU_RUN_TEST(test_orchestrator_merge_consensus_divergent_keeps_all);
    HU_RUN_TEST(test_orchestrator_merge_consensus_single_task);
    HU_RUN_TEST(test_orchestrator_merge_consensus_fresh_no_tasks_returns_empty);
    HU_RUN_TEST(test_orchestrator_merge_consensus_all_failed_returns_empty);
    HU_RUN_TEST(test_orchestrator_merge_results_empty_completed_returns_ok);

    HU_TEST_SUITE("Multi-Agent Coordination");
    HU_RUN_TEST(test_multi_agent_mailbox_task_delegation_and_reply);
    HU_RUN_TEST(test_multi_agent_orchestrator_decompose_assign_merge);
    HU_RUN_TEST(test_multi_agent_broadcast_reaches_all_workers);
}
