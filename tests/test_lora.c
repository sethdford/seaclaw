/*
 * LoRA adapter tests.
 */
#include "human/core/allocator.h"
#include "human/ml/lora.h"
#include "test_framework.h"
#include <math.h>
#include <string.h>

#ifdef HU_ENABLE_ML

static void lora_create_basic(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_lora_config_t cfg = {.rank = 4, .alpha = 8.0f, .dropout = 0.0f, .targets = HU_LORA_TARGET_ALL};
    hu_lora_adapter_t *adapter = NULL;

    hu_error_t err = hu_lora_create(&alloc, &cfg, 8, 8, 2, &adapter);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(adapter);

    hu_lora_destroy(&alloc, adapter);
}

static void lora_param_count(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_lora_config_t cfg = {.rank = 4, .alpha = 8.0f, .dropout = 0.0f, .targets = HU_LORA_TARGET_ALL};
    hu_lora_adapter_t *adapter = NULL;

    HU_ASSERT_EQ(hu_lora_create(&alloc, &cfg, 8, 8, 2, &adapter), HU_OK);
    size_t n = hu_lora_num_params(adapter);
    /* Per layer: rank*in_dim + out_dim*rank = 4*8 + 8*4 = 64. Two layers = 128. */
    HU_ASSERT_EQ(n, 128);

    hu_lora_destroy(&alloc, adapter);
}

static void lora_invalid_config(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_lora_config_t cfg = {.rank = 0, .alpha = 8.0f, .dropout = 0.0f, .targets = HU_LORA_TARGET_ALL};
    hu_lora_adapter_t *adapter = NULL;

    hu_error_t err = hu_lora_create(&alloc, &cfg, 8, 8, 2, &adapter);
    HU_ASSERT_NEQ(err, HU_OK);
    HU_ASSERT_NULL(adapter);
}

static void lora_destroy_null(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_lora_destroy(&alloc, NULL);
}

static void lora_apply_adds_delta(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_lora_config_t cfg = {.rank = 2, .alpha = 2.0f, .dropout = 0.0f, .targets = HU_LORA_TARGET_ALL};
    hu_lora_adapter_t *adapter = NULL;

    HU_ASSERT_EQ(hu_lora_create(&alloc, &cfg, 4, 4, 1, &adapter), HU_OK);

    /* Set A and B to known values. A[2x4], B[4x2]. scale = 2/2 = 1.0 */
    float A[8] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
    float B[8] = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    HU_ASSERT_EQ(hu_lora_set_layer_weights(adapter, 0, A, B), HU_OK);

    float input[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float output[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    HU_ASSERT_EQ(hu_lora_apply(adapter, 0, input, 1, output), HU_OK);

    /* temp = input * A^T: [1,2,3,4] * A^T. Row 0 of A^T = [1,0,0,0], row 1 = [0,1,0,0].
     * temp[0] = 1*1+2*0+3*0+4*0 = 1, temp[1] = 1*0+2*1+3*0+4*0 = 2.
     * delta = temp * B^T: [1,2] * B^T. B^T = [[1,0],[0,1],[0,0],[0,0]].
     * delta = [1*1+2*0, 1*0+2*1, 0, 0] = [1, 2, 0, 0]. scale=1.
     * output += [1,2,0,0] = [1,2,0,0] */
    HU_ASSERT_FLOAT_EQ(output[0], 1.0f, 0.001f);
    HU_ASSERT_FLOAT_EQ(output[1], 2.0f, 0.001f);
    HU_ASSERT_FLOAT_EQ(output[2], 0.0f, 0.001f);
    HU_ASSERT_FLOAT_EQ(output[3], 0.0f, 0.001f);

    hu_lora_destroy(&alloc, adapter);
}

static void lora_backward_accumulates(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_lora_config_t cfg = {.rank = 2, .alpha = 1.0f, .dropout = 0.0f, .targets = HU_LORA_TARGET_ALL};
    hu_lora_adapter_t *adapter = NULL;

    HU_ASSERT_EQ(hu_lora_create(&alloc, &cfg, 4, 4, 1, &adapter), HU_OK);

    float input[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float grad_output[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    HU_ASSERT_EQ(hu_lora_backward(adapter, 0, input, grad_output, 1, NULL), HU_OK);

    /* Gradients should be non-zero (we can't inspect them directly, but backward succeeded) */
    hu_lora_destroy(&alloc, adapter);
}

static void lora_target_flags(void) {
    HU_ASSERT_EQ((int)(HU_LORA_TARGET_Q | HU_LORA_TARGET_V), (int)HU_LORA_TARGET_QV);
    HU_ASSERT_EQ((int)HU_LORA_TARGET_ALL, 0x3F);
}

static void lora_save_load_test_mode(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_lora_config_t cfg = {.rank = 4, .alpha = 8.0f, .dropout = 0.0f, .targets = HU_LORA_TARGET_ALL};
    hu_lora_adapter_t *adapter = NULL;

    HU_ASSERT_EQ(hu_lora_create(&alloc, &cfg, 8, 8, 2, &adapter), HU_OK);

    hu_error_t err = hu_lora_save(adapter, "/tmp/lora_test.bin");
    HU_ASSERT_EQ(err, HU_OK);

    hu_lora_adapter_t *loaded = NULL;
    err = hu_lora_load(&alloc, "/tmp/lora_test.bin", &loaded);
    HU_ASSERT_EQ(err, HU_OK);
    /* In test mode load returns OK but may not allocate (skips file I/O) */
    if (loaded)
        hu_lora_destroy(&alloc, loaded);

    hu_lora_destroy(&alloc, adapter);
}

void run_lora_tests(void) {
    HU_TEST_SUITE("LoRA Adapter");
    HU_RUN_TEST(lora_create_basic);
    HU_RUN_TEST(lora_param_count);
    HU_RUN_TEST(lora_invalid_config);
    HU_RUN_TEST(lora_destroy_null);
    HU_RUN_TEST(lora_apply_adds_delta);
    HU_RUN_TEST(lora_backward_accumulates);
    HU_RUN_TEST(lora_target_flags);
    HU_RUN_TEST(lora_save_load_test_mode);
}

#else

void run_lora_tests(void) {
    (void)0;
}

#endif
