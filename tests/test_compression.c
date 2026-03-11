#include "human/core/allocator.h"
#include "human/core/string.h"
#include "human/memory/compression.h"
#include "test_framework.h"
#include <string.h>

static void test_compression_create_table_sql_valid(void) {
    char buf[1024];
    size_t len = 0;
    hu_error_t err = hu_compression_create_table_sql(buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_TRUE(strstr(buf, "CREATE TABLE") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "shared_references") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "UNIQUE") != NULL);
}

static void test_compression_insert_sql_escapes_quotes(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_shared_ref_t ref = {
        .id = 1,
        .contact_id = hu_strndup(&alloc, "alice", 5),
        .contact_id_len = 5,
        .compressed_form = hu_strndup(&alloc, "O'Brien's incident", 18),
        .compressed_form_len = 18,
        .expanded_meaning = hu_strndup(&alloc, "the food poisoning", 17),
        .expanded_meaning_len = 17,
        .usage_count = 2,
        .created_at = 1000,
        .last_used_at = 2000,
        .strength = 0.8,
        .stage = HU_COMPRESS_FULL,
    };
    char buf[512];
    size_t len = 0;
    hu_error_t err = hu_compression_insert_sql(&ref, buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(strstr(buf, "''") != NULL);
    hu_shared_ref_deinit(&alloc, &ref);
}

static void test_compression_query_sql_with_contact(void) {
    char buf[512];
    size_t len = 0;
    hu_error_t err = hu_compression_query_sql("user_123", 8, buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(strstr(buf, "user_123") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "WHERE") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "ORDER BY strength DESC") != NULL);
}

static void test_compression_record_usage_sql_valid(void) {
    char buf[256];
    size_t len = 0;
    hu_error_t err = hu_compression_record_usage_sql(42, 1234567890ULL, buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(strstr(buf, "usage_count = usage_count + 1") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "last_used_at") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "42") != NULL);
}

static void test_compression_find_in_message_single_match(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_shared_ref_t refs[1] = {{
        .id = 1,
        .contact_id = hu_strndup(&alloc, "alice", 5),
        .contact_id_len = 5,
        .compressed_form = hu_strndup(&alloc, "mango", 5),
        .compressed_form_len = 5,
        .expanded_meaning = hu_strndup(&alloc, "food poisoning", 14),
        .expanded_meaning_len = 14,
        .usage_count = 3,
        .strength = 0.9,
        .stage = HU_COMPRESS_SINGLE_WORD,
    }};
    const char *msg = "Remember the mango?";
    size_t match_indices[4];
    size_t n = hu_compression_find_in_message(refs, 1, msg, strlen(msg), match_indices, 4);
    HU_ASSERT_EQ(n, 1);

    HU_ASSERT_EQ(match_indices[0], 0);
    hu_shared_ref_deinit(&alloc, &refs[0]);
}

static void test_compression_find_in_message_multiple(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_shared_ref_t refs[3] = {
        {
            .id = 1,
            .contact_id = hu_strndup(&alloc, "alice", 5),
            .contact_id_len = 5,
            .compressed_form = hu_strndup(&alloc, "mango", 5),
            .compressed_form_len = 5,
            .expanded_meaning = hu_strndup(&alloc, "food poisoning", 14),
            .expanded_meaning_len = 14,
            .usage_count = 3,
            .strength = 0.9,
            .stage = HU_COMPRESS_SINGLE_WORD,
        },
        {
            .id = 2,
            .contact_id = hu_strndup(&alloc, "alice", 5),
            .contact_id_len = 5,
            .compressed_form = hu_strndup(&alloc, "your nemesis", 12),
            .compressed_form_len = 12,
            .expanded_meaning = hu_strndup(&alloc, "Kevin", 5),
            .expanded_meaning_len = 5,
            .usage_count = 2,
            .strength = 0.8,
            .stage = HU_COMPRESS_SINGLE_WORD,
        },
        {
            .id = 3,
            .contact_id = hu_strndup(&alloc, "alice", 5),
            .contact_id_len = 5,
            .compressed_form = hu_strndup(&alloc, "xyz", 3),
            .compressed_form_len = 3,
            .expanded_meaning = hu_strndup(&alloc, "none", 4),
            .expanded_meaning_len = 4,
            .usage_count = 1,
            .strength = 0.5,
            .stage = HU_COMPRESS_FULL,
        },
    };
    const char *msg = "Your nemesis and the mango incident";
    size_t match_indices[4];
    size_t n = hu_compression_find_in_message(refs, 3, msg, strlen(msg), match_indices, 4);
    HU_ASSERT_EQ(n, 2);

    hu_shared_ref_deinit(&alloc, &refs[0]);
    hu_shared_ref_deinit(&alloc, &refs[1]);
    hu_shared_ref_deinit(&alloc, &refs[2]);
}

static void test_compression_find_in_message_none(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_shared_ref_t refs[1] = {{
        .id = 1,
        .contact_id = hu_strndup(&alloc, "alice", 5),
        .contact_id_len = 5,
        .compressed_form = hu_strndup(&alloc, "mango", 5),
        .compressed_form_len = 5,
        .expanded_meaning = hu_strndup(&alloc, "food poisoning", 14),
        .expanded_meaning_len = 14,
        .usage_count = 3,
        .strength = 0.9,
        .stage = HU_COMPRESS_SINGLE_WORD,
    }};
    const char *msg = "Nothing special here";
    size_t match_indices[4];
    size_t n = hu_compression_find_in_message(refs, 1, msg, strlen(msg), match_indices, 4);
    HU_ASSERT_EQ(n, 0);
    hu_shared_ref_deinit(&alloc, &refs[0]);
}

static void test_compression_find_in_message_case_insensitive(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_shared_ref_t refs[1] = {{
        .id = 1,
        .contact_id = hu_strndup(&alloc, "alice", 5),
        .contact_id_len = 5,
        .compressed_form = hu_strndup(&alloc, "mango", 5),
        .compressed_form_len = 5,
        .expanded_meaning = hu_strndup(&alloc, "food poisoning", 14),
        .expanded_meaning_len = 14,
        .usage_count = 3,
        .strength = 0.9,
        .stage = HU_COMPRESS_SINGLE_WORD,
    }};
    const char *msg = "Remember the MANGO?";
    size_t match_indices[4];
    size_t n = hu_compression_find_in_message(refs, 1, msg, strlen(msg), match_indices, 4);
    HU_ASSERT_EQ(n, 1);
    hu_shared_ref_deinit(&alloc, &refs[0]);
}

static void test_compression_should_deploy_true(void) {
    hu_shared_ref_t ref = {
        .id = 1,
        .strength = 0.8,
        .usage_count = 5,
    };
    hu_compression_config_t config = {
        .min_uses_to_compress = 2,
        .max_refs_per_contact = 20,
        .deployment_probability = 0.9,
        .never_during_conflict = true,
        .strength_decay_rate = 0.02,
        .min_strength_to_deploy = 0.4,
    };
    /* Seed that yields r < 0.9 for deployment_probability */
    bool deployed = hu_compression_should_deploy(&ref, &config, false, 1);
    HU_ASSERT_TRUE(deployed);
}

static void test_compression_should_deploy_false_during_conflict(void) {
    hu_shared_ref_t ref = {
        .id = 1,
        .strength = 0.9,
        .usage_count = 10,
    };
    hu_compression_config_t config = {
        .min_uses_to_compress = 2,
        .max_refs_per_contact = 20,
        .deployment_probability = 1.0,
        .never_during_conflict = true,
        .strength_decay_rate = 0.02,
        .min_strength_to_deploy = 0.4,
    };
    bool deployed = hu_compression_should_deploy(&ref, &config, true, 0);
    HU_ASSERT_FALSE(deployed);
}

static void test_compression_should_deploy_false_weak_ref(void) {
    hu_shared_ref_t ref = {
        .id = 1,
        .strength = 0.2,
        .usage_count = 2,
    };
    hu_compression_config_t config = {
        .min_uses_to_compress = 2,
        .max_refs_per_contact = 20,
        .deployment_probability = 1.0,
        .never_during_conflict = true,
        .strength_decay_rate = 0.02,
        .min_strength_to_deploy = 0.4,
    };
    bool deployed = hu_compression_should_deploy(&ref, &config, false, 0);
    HU_ASSERT_FALSE(deployed);
}

static void test_compression_decay_strength_one_week(void) {
    uint64_t now = 14ULL * 86400ULL * 1000ULL;
    uint64_t last_used = 7ULL * 86400ULL * 1000ULL;
    double result = hu_compression_decay_strength(0.5, last_used, now, 0.02);
    HU_ASSERT_FLOAT_EQ(result, 0.48, 0.001);
}

static void test_compression_decay_strength_no_time(void) {
    uint64_t now = 1000;
    uint64_t last_used = 1000;
    double result = hu_compression_decay_strength(0.7, last_used, now, 0.02);
    HU_ASSERT_FLOAT_EQ(result, 0.7, 0.001);
}

static void test_compression_decay_strength_clamps_zero(void) {
    uint64_t now = 100ULL * 7ULL * 86400ULL * 1000ULL;
    uint64_t last_used = 0;
    double result = hu_compression_decay_strength(0.1, last_used, now, 0.02);
    HU_ASSERT_FLOAT_EQ(result, 0.0, 0.001);
}

static void test_compression_should_advance_true(void) {
    hu_shared_ref_t ref = {
        .usage_count = 1,
        .stage = HU_COMPRESS_FULL,
    };
    bool advance = hu_compression_should_advance(&ref, 1);
    HU_ASSERT_TRUE(advance);
}

static void test_compression_should_advance_false_not_enough(void) {
    hu_shared_ref_t ref = {
        .usage_count = 2,
        .stage = HU_COMPRESS_ABBREVIATED,
    };
    bool advance = hu_compression_should_advance(&ref, 2);
    HU_ASSERT_FALSE(advance);
}

static void test_compression_should_advance_false_max_stage(void) {
    hu_shared_ref_t ref = {
        .usage_count = 100,
        .stage = HU_COMPRESS_SINGLE_WORD,
    };
    bool advance = hu_compression_should_advance(&ref, 1);
    HU_ASSERT_FALSE(advance);
}

static void test_compression_build_prompt_with_refs(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_shared_ref_t refs[2] = {
        {
            .id = 1,
            .contact_id = hu_strndup(&alloc, "alice", 5),
            .contact_id_len = 5,
            .compressed_form = hu_strndup(&alloc, "mango", 5),
            .compressed_form_len = 5,
            .expanded_meaning = hu_strndup(&alloc, "the food poisoning incident in Mexico", 36),
            .expanded_meaning_len = 36,
            .usage_count = 7,
            .strength = 0.90,
            .stage = HU_COMPRESS_SINGLE_WORD,
        },
        {
            .id = 2,
            .contact_id = hu_strndup(&alloc, "alice", 5),
            .contact_id_len = 5,
            .compressed_form = hu_strndup(&alloc, "your nemesis", 12),
            .compressed_form_len = 12,
            .expanded_meaning = hu_strndup(&alloc, "their coworker Kevin", 19),
            .expanded_meaning_len = 19,
            .usage_count = 4,
            .strength = 0.80,
            .stage = HU_COMPRESS_SINGLE_WORD,
        },
    };
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_compression_build_prompt(&alloc, refs, 2, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "mango") != NULL);
    HU_ASSERT_TRUE(strstr(out, "your nemesis") != NULL);
    HU_ASSERT_TRUE(strstr(out, "SHARED LANGUAGE") != NULL);
    hu_str_free(&alloc, out);
    hu_shared_ref_deinit(&alloc, &refs[0]);
    hu_shared_ref_deinit(&alloc, &refs[1]);
}

static void test_compression_build_prompt_empty(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_compression_build_prompt(&alloc, NULL, 0, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "No shared references") != NULL);
    hu_str_free(&alloc, out);
}

static void test_compression_shared_ref_deinit_frees_all(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_shared_ref_t ref = {0};
    ref.contact_id = hu_strndup(&alloc, "alice", 5);
    ref.contact_id_len = 5;
    ref.compressed_form = hu_strndup(&alloc, "mango", 5);
    ref.compressed_form_len = 5;
    ref.expanded_meaning = hu_strndup(&alloc, "food poisoning", 14);
    ref.expanded_meaning_len = 14;

    hu_shared_ref_deinit(&alloc, &ref);

    HU_ASSERT_NULL(ref.contact_id);
    HU_ASSERT_NULL(ref.compressed_form);
    HU_ASSERT_NULL(ref.expanded_meaning);
}

void run_compression_tests(void) {
    HU_TEST_SUITE("compression");
    HU_RUN_TEST(test_compression_create_table_sql_valid);
    HU_RUN_TEST(test_compression_insert_sql_escapes_quotes);
    HU_RUN_TEST(test_compression_query_sql_with_contact);
    HU_RUN_TEST(test_compression_record_usage_sql_valid);
    HU_RUN_TEST(test_compression_find_in_message_single_match);
    HU_RUN_TEST(test_compression_find_in_message_multiple);
    HU_RUN_TEST(test_compression_find_in_message_none);
    HU_RUN_TEST(test_compression_find_in_message_case_insensitive);
    HU_RUN_TEST(test_compression_should_deploy_true);
    HU_RUN_TEST(test_compression_should_deploy_false_during_conflict);
    HU_RUN_TEST(test_compression_should_deploy_false_weak_ref);
    HU_RUN_TEST(test_compression_decay_strength_one_week);
    HU_RUN_TEST(test_compression_decay_strength_no_time);
    HU_RUN_TEST(test_compression_decay_strength_clamps_zero);
    HU_RUN_TEST(test_compression_should_advance_true);
    HU_RUN_TEST(test_compression_should_advance_false_not_enough);
    HU_RUN_TEST(test_compression_should_advance_false_max_stage);
    HU_RUN_TEST(test_compression_build_prompt_with_refs);
    HU_RUN_TEST(test_compression_build_prompt_empty);
    HU_RUN_TEST(test_compression_shared_ref_deinit_frees_all);
}
