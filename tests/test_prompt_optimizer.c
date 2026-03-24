#include "human/agent/prompt_optimizer.h"
#include "test_framework.h"
#include <string.h>

static void test_optimizer_init(void) {
    hu_prompt_optimizer_t opt;
    hu_prompt_optimizer_init(&opt);
    HU_ASSERT_EQ((int)opt.example_count, 0);
    HU_ASSERT_EQ((int)opt.max_iterations, 5);
    HU_ASSERT_EQ((int)opt.iterations_run, 0);
}

static void test_signature_add_input(void) {
    hu_prompt_signature_t sig;
    memset(&sig, 0, sizeof(sig));
    HU_ASSERT_EQ(hu_prompt_signature_add_input(&sig, "query", "The user query", true), HU_OK);
    HU_ASSERT_EQ((int)sig.input_count, 1);
    HU_ASSERT_STR_EQ(sig.inputs[0].name, "query");
    HU_ASSERT_TRUE(sig.inputs[0].required);
}

static void test_signature_add_output(void) {
    hu_prompt_signature_t sig;
    memset(&sig, 0, sizeof(sig));
    HU_ASSERT_EQ(hu_prompt_signature_add_output(&sig, "answer", "The response", true), HU_OK);
    HU_ASSERT_EQ((int)sig.output_count, 1);
    HU_ASSERT_STR_EQ(sig.outputs[0].name, "answer");
}

static void test_optimizer_add_example(void) {
    hu_prompt_optimizer_t opt;
    hu_prompt_optimizer_init(&opt);
    hu_prompt_example_t ex = {
        .input = "What is 2+2?",
        .input_len = 12,
        .expected_output = "4",
        .expected_output_len = 1,
        .score = 1.0,
    };
    HU_ASSERT_EQ(hu_prompt_optimizer_add_example(&opt, &ex), HU_OK);
    HU_ASSERT_EQ((int)opt.example_count, 1);
}

static void test_optimizer_compile(void) {
    hu_prompt_optimizer_t opt;
    hu_prompt_optimizer_init(&opt);
    memcpy(opt.signature.instruction, "Answer questions accurately.", 28);
    hu_prompt_signature_add_input(&opt.signature, "question", "User question", true);
    hu_prompt_signature_add_output(&opt.signature, "answer", "Your answer", true);

    hu_prompt_example_t ex = {
        .input = "What is 2+2?",
        .input_len = 12,
        .expected_output = "4",
        .expected_output_len = 1,
        .score = 0.9,
    };
    hu_prompt_optimizer_add_example(&opt, &ex);

    char out[2048];
    size_t out_len = 0;
    HU_ASSERT_EQ(hu_prompt_optimizer_compile(&opt, out, sizeof(out), &out_len), HU_OK);
    HU_ASSERT_TRUE(out_len > 0);
    HU_ASSERT_STR_CONTAINS(out, "Answer questions accurately");
    HU_ASSERT_STR_CONTAINS(out, "question");
    HU_ASSERT_STR_CONTAINS(out, "answer");
    HU_ASSERT_STR_CONTAINS(out, "Example:");
    HU_ASSERT_STR_CONTAINS(out, "2+2");
}

static void test_optimizer_compile_empty(void) {
    hu_prompt_optimizer_t opt;
    hu_prompt_optimizer_init(&opt);
    char out[512];
    size_t out_len = 0;
    HU_ASSERT_EQ(hu_prompt_optimizer_compile(&opt, out, sizeof(out), &out_len), HU_OK);
}

static void test_optimizer_optimize_mock(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_prompt_optimizer_t opt;
    hu_prompt_optimizer_init(&opt);
    memcpy(opt.signature.instruction, "Test prompt", 12);
    hu_prompt_signature_add_input(&opt.signature, "q", "question", true);

    hu_provider_t provider = {0};
    hu_prompt_example_t golden[] = {
        {.input = "test",
         .input_len = 4,
         .expected_output = "result",
         .expected_output_len = 6,
         .score = 0.0},
    };

    HU_ASSERT_EQ(hu_prompt_optimizer_optimize(&opt, &alloc, &provider, "m", 1, golden, 1), HU_OK);
    HU_ASSERT_TRUE(opt.best_score > 0.0);
    HU_ASSERT_TRUE(opt.iterations_run > 0);
}

static void test_optimizer_null_args(void) {
    HU_ASSERT_EQ(hu_prompt_optimizer_compile(NULL, NULL, 0, NULL), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_prompt_optimizer_optimize(NULL, NULL, NULL, NULL, 0, NULL, 0),
                 HU_ERR_INVALID_ARGUMENT);
}

static void test_signature_add_null(void) {
    HU_ASSERT_EQ(hu_prompt_signature_add_input(NULL, NULL, NULL, false), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_prompt_signature_add_output(NULL, NULL, NULL, false), HU_ERR_INVALID_ARGUMENT);
}

static void test_optimizer_add_example_null(void) {
    HU_ASSERT_EQ(hu_prompt_optimizer_add_example(NULL, NULL), HU_ERR_INVALID_ARGUMENT);
}

void run_prompt_optimizer_tests(void) {
    HU_TEST_SUITE("Prompt Optimizer");
    HU_RUN_TEST(test_optimizer_init);
    HU_RUN_TEST(test_signature_add_input);
    HU_RUN_TEST(test_signature_add_output);
    HU_RUN_TEST(test_optimizer_add_example);
    HU_RUN_TEST(test_optimizer_compile);
    HU_RUN_TEST(test_optimizer_compile_empty);
    HU_RUN_TEST(test_optimizer_optimize_mock);
    HU_RUN_TEST(test_optimizer_null_args);
    HU_RUN_TEST(test_signature_add_null);
    HU_RUN_TEST(test_optimizer_add_example_null);
}
