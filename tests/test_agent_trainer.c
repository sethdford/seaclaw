/* Agent trainer offline training loop tests (AGI-V4). */

#ifdef HU_ENABLE_ML

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

static void trainer_converts_trajectories(void)
{
    hu_allocator_t alloc = hu_system_allocator();
    const char *json = "{\"state\":\"user wants help\",\"action\":\"provide answer\",\"reward\":0.8},"
                       "{\"state\":\"follow up\",\"action\":\"clarify\",\"reward\":0.6}";
    hu_training_triple_t triples[4];
    size_t count = 0;

    hu_error_t err = hu_training_convert_trajectory(&alloc, json, strlen(json),
                                                     triples, 4, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, 2u);
    HU_ASSERT_TRUE(triples[0].prompt_len > 0);
    HU_ASSERT_TRUE(triples[0].response_len > 0);
    HU_ASSERT_TRUE(triples[0].reward >= 0.7);
    HU_ASSERT_TRUE(triples[1].reward >= 0.5);
}

static void trainer_replay_buffer_lifecycle(void)
{
    hu_allocator_t alloc = hu_system_allocator();
    hu_replay_buffer_t buf;
    HU_ASSERT_EQ(hu_replay_buffer_create(&alloc, 8, &buf), HU_OK);
    HU_ASSERT_EQ(buf.capacity, 8u);
    HU_ASSERT_EQ(buf.count, 0u);

    hu_training_triple_t t = {0};
    memcpy(t.prompt, "state1", 6);
    t.prompt_len = 6;
    memcpy(t.response, "act1", 4);
    t.response_len = 4;
    t.reward = 0.9;
    HU_ASSERT_EQ(hu_replay_buffer_add(&buf, &t), HU_OK);
    HU_ASSERT_EQ(buf.count, 1u);

    t.reward = 0.1;
    HU_ASSERT_EQ(hu_replay_buffer_add(&buf, &t), HU_OK);
    HU_ASSERT_EQ(buf.count, 2u);

    hu_training_triple_t sampled[4];
    size_t out_count = 0;
    HU_ASSERT_EQ(hu_replay_buffer_sample(&buf, 2, 1.0, sampled, &out_count), HU_OK);
    HU_ASSERT_EQ(out_count, 2u);
    /* First sample should be higher-reward due to weighting */
    HU_ASSERT_TRUE(sampled[0].reward >= sampled[1].reward);

    hu_replay_buffer_destroy(&alloc, &buf);
}

static void trainer_replay_buffer_wraps(void)
{
    hu_allocator_t alloc = hu_system_allocator();
    hu_replay_buffer_t buf;
    HU_ASSERT_EQ(hu_replay_buffer_create(&alloc, 4, &buf), HU_OK);

    for (int i = 0; i < 6; i++) {
        hu_training_triple_t t = {0};
        t.reward = (double)i * 0.1;
        HU_ASSERT_EQ(hu_replay_buffer_add(&buf, &t), HU_OK);
    }
    HU_ASSERT_EQ(buf.count, 4u);

    hu_replay_buffer_destroy(&alloc, &buf);
}

static void trainer_checkpoint_roundtrip(void)
{
    hu_allocator_t alloc = hu_system_allocator();
    hu_training_checkpoint_t ckpt = {
        .step = 500,
        .loss = 0.125,
        .avg_reward = 0.85,
        .trajectories_seen = 1000,
        .valid = true,
    };
    memcpy(ckpt.model_path, "/tmp/model.bin", 14);

    char buf[512];
    size_t out_len = 0;
    HU_ASSERT_EQ(hu_training_save_checkpoint(&alloc, &ckpt, buf, sizeof(buf), &out_len), HU_OK);
    HU_ASSERT_TRUE(out_len > 0);

    hu_training_checkpoint_t loaded = {0};
    HU_ASSERT_EQ(hu_training_load_checkpoint(&alloc, buf, out_len, &loaded), HU_OK);
    HU_ASSERT_EQ(loaded.step, 500u);
    HU_ASSERT_TRUE(loaded.loss >= 0.124 && loaded.loss <= 0.126);
    HU_ASSERT_TRUE(loaded.avg_reward >= 0.84 && loaded.avg_reward <= 0.86);
    HU_ASSERT_EQ(loaded.trajectories_seen, 1000u);
    HU_ASSERT_TRUE(loaded.valid);
    HU_ASSERT_STR_EQ(loaded.model_path, "/tmp/model.bin");
}

static void trainer_convert_null_args(void)
{
    hu_allocator_t alloc = hu_system_allocator();
    hu_training_triple_t triples[2];
    size_t count = 0;

    HU_ASSERT_EQ(hu_training_convert_trajectory(&alloc, NULL, 0, triples, 2, &count),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_training_convert_trajectory(&alloc, "x", 1, NULL, 2, &count),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_training_convert_trajectory(&alloc, "x", 1, triples, 0, &count),
                 HU_ERR_INVALID_ARGUMENT);
}

static void trainer_replay_null_args(void)
{
    hu_training_triple_t t = {0};
    hu_allocator_t alloc = hu_system_allocator();
    hu_replay_buffer_t buf;
    HU_ASSERT_EQ(hu_replay_buffer_create(NULL, 8, &buf), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_replay_buffer_create(&alloc, 0, &buf), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_replay_buffer_add(NULL, &t), HU_ERR_INVALID_ARGUMENT);
}

static void trainer_collector_lifecycle(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_training_collector_t tc;
    HU_ASSERT_EQ(hu_training_collector_init(&alloc, &tc, 8), HU_OK);
    HU_ASSERT_TRUE(tc.enabled);
    HU_ASSERT_EQ(tc.capacity, 8u);

    HU_ASSERT_EQ(hu_training_collector_record(&tc, "user said hello", 15,
                                               "responded with greeting", 23, 0.9), HU_OK);
    HU_ASSERT_EQ(tc.count, 1u);
    HU_ASSERT_EQ(tc.buffer[0].reward, 0.9);

    HU_ASSERT_EQ(hu_training_collector_record(&tc, "user asked Q", 12,
                                               "answered correctly", 18, 1.0), HU_OK);
    HU_ASSERT_EQ(tc.count, 2u);

    char buf[4096];
    size_t out_len = 0;
    HU_ASSERT_EQ(hu_training_collector_export_json(&alloc, &tc, buf, sizeof(buf), &out_len), HU_OK);
    HU_ASSERT_TRUE(out_len > 0);
    HU_ASSERT_TRUE(strstr(buf, "\"count\":2") != NULL);

    hu_training_collector_destroy(&alloc, &tc);
    HU_ASSERT_NULL(tc.buffer);
    HU_ASSERT_FALSE(tc.enabled);
}

static void trainer_collector_wraps(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_training_collector_t tc;
    hu_training_collector_init(&alloc, &tc, 4);

    for (int i = 0; i < 6; i++) {
        hu_training_collector_record(&tc, "s", 1, "a", 1, (double)i * 0.1);
    }
    HU_ASSERT_EQ(tc.count, 6u);

    hu_training_collector_destroy(&alloc, &tc);
}

static void trainer_collector_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_training_collector_t tc;
    HU_ASSERT_EQ(hu_training_collector_init(NULL, &tc, 8), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_training_collector_init(&alloc, NULL, 8), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_training_collector_record(NULL, "s", 1, "a", 1, 0.0), HU_ERR_INVALID_ARGUMENT);
}

void run_agent_trainer_tests(void)
{
    HU_TEST_SUITE("agent_trainer");
    HU_RUN_TEST(trainer_config_defaults);
    HU_RUN_TEST(trainer_step_produces_metrics);
    HU_RUN_TEST(trainer_loss_decreases);
    HU_RUN_TEST(trainer_metrics_report);
    HU_RUN_TEST(trainer_null_args_returns_error);
    HU_RUN_TEST(trainer_converts_trajectories);
    HU_RUN_TEST(trainer_replay_buffer_lifecycle);
    HU_RUN_TEST(trainer_replay_buffer_wraps);
    HU_RUN_TEST(trainer_checkpoint_roundtrip);
    HU_RUN_TEST(trainer_convert_null_args);
    HU_RUN_TEST(trainer_replay_null_args);
    HU_RUN_TEST(trainer_collector_lifecycle);
    HU_RUN_TEST(trainer_collector_wraps);
    HU_RUN_TEST(trainer_collector_null_args);
}

#else

void run_agent_trainer_tests(void) { (void)0; }

#endif /* HU_ENABLE_ML */
