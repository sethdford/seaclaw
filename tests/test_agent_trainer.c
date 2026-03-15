/* Agent trainer offline training loop tests (AGI-V4). */

#include "human/core/allocator.h"
#include "human/ml/agent_trainer.h"
#include "test_framework.h"
#include <string.h>

static void trainer_config_defaults(void)
{
    hu_agent_training_config_t c = hu_training_config_default();
    HU_ASSERT_EQ(c.batch_size, 32u);
    HU_ASSERT_EQ(c.learning_rate, 0.001);
    HU_ASSERT_EQ(c.max_steps, 1000u);
    HU_ASSERT_EQ(c.reward_weight, 1.0);
    HU_ASSERT_EQ(c.replay_buffer, 1024u);
}

static void trainer_step_produces_metrics(void)
{
    hu_allocator_t alloc = hu_system_allocator();
    hu_agent_training_config_t config = hu_training_config_default();
    const char *json = "[{\"id\":1,\"total_reward\":0.5,\"steps\":[{\"state\":\"s1\",\"action\":\"a1\",\"reward\":0.5}]}]";
    size_t json_len = strlen(json);
    hu_training_metrics_t metrics = {0};

    hu_error_t err = hu_agent_train_step(&alloc, &config, json, json_len, &metrics);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(metrics.loss > 0.0);
    HU_ASSERT_TRUE(metrics.steps_completed >= 1u);
}

static void trainer_loss_decreases(void)
{
    hu_allocator_t alloc = hu_system_allocator();
    hu_agent_training_config_t config = hu_training_config_default();
    hu_training_metrics_t m1 = {0}, m2 = {0};

    const char *json_few = "[{\"steps\":[{\"reward\":0.1},{\"reward\":0.2}]}]";
    hu_error_t err = hu_agent_train_step(&alloc, &config, json_few, strlen(json_few), &m1);
    HU_ASSERT_EQ(err, HU_OK);

    const char *json_many = "[{\"steps\":[{\"reward\":0.1},{\"reward\":0.2},{\"reward\":0.3},{\"reward\":0.4},{\"reward\":0.5},{\"reward\":0.6},{\"reward\":0.7},{\"reward\":0.8},{\"reward\":0.9},{\"reward\":1.0},{\"reward\":1.1},{\"reward\":1.2}]}]";
    err = hu_agent_train_step(&alloc, &config, json_many, strlen(json_many), &m2);
    HU_ASSERT_EQ(err, HU_OK);

    HU_ASSERT_TRUE(m2.loss < m1.loss);
}

static void trainer_metrics_report(void)
{
    hu_allocator_t alloc = hu_system_allocator();
    hu_training_metrics_t m = {
        .loss = 0.5,
        .avg_reward = 0.75,
        .steps_completed = 100,
        .trajectories_used = 5,
        .converging = true,
    };
    char buf[256];
    size_t out_len = 0;

    hu_error_t err = hu_training_metrics_report(&alloc, &m, buf, sizeof(buf), &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(out_len > 0);
    HU_ASSERT_TRUE(strstr(buf, "Loss:") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "Converging:") != NULL);
}

static void trainer_null_args_returns_error(void)
{
    hu_allocator_t alloc = hu_system_allocator();
    hu_agent_training_config_t config = hu_training_config_default();
    const char *json = "[]";
    hu_training_metrics_t metrics = {0};
    char buf[64];
    size_t out_len = 0;

    hu_error_t err = hu_agent_train_step(&alloc, &config, NULL, 0, &metrics);
    HU_ASSERT_NEQ(err, HU_OK);

    err = hu_agent_train_step(&alloc, &config, json, 2, NULL);
    HU_ASSERT_NEQ(err, HU_OK);

    err = hu_training_metrics_report(&alloc, NULL, buf, sizeof(buf), &out_len);
    HU_ASSERT_NEQ(err, HU_OK);

    err = hu_training_metrics_report(&alloc, &metrics, NULL, sizeof(buf), &out_len);
    HU_ASSERT_NEQ(err, HU_OK);

    err = hu_training_metrics_report(&alloc, &metrics, buf, sizeof(buf), NULL);
    HU_ASSERT_NEQ(err, HU_OK);
}

void run_agent_trainer_tests(void)
{
    HU_TEST_SUITE("agent_trainer");
    HU_RUN_TEST(trainer_config_defaults);
    HU_RUN_TEST(trainer_step_produces_metrics);
    HU_RUN_TEST(trainer_loss_decreases);
    HU_RUN_TEST(trainer_metrics_report);
    HU_RUN_TEST(trainer_null_args_returns_error);
}
