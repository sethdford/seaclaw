#include "human/core/allocator.h"
#include "human/core/string.h"
#include "human/memory/cognitive.h"
#include "test_framework.h"
#include <string.h>

static void test_opinions_create_table_valid(void) {
    char buf[1024];
    size_t len = 0;
    hu_error_t err = hu_opinions_create_table_sql(buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_TRUE(strstr(buf, "CREATE TABLE") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "opinions") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "superseded_by") != NULL);
}

static void test_opinions_upsert_sql_valid(void) {
    char buf[512];
    size_t len = 0;
    hu_error_t err = hu_opinions_upsert_sql("pizza", 5, "delicious", 9, 0.9, 1000u,
                                            buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_TRUE(strstr(buf, "INSERT INTO") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "pizza") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "delicious") != NULL);
}

static void test_opinions_query_current_sql_valid(void) {
    char buf[512];
    size_t len = 0;
    hu_error_t err = hu_opinions_query_current_sql("pizza", 5, buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(strstr(buf, "superseded_by IS NULL") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "WHERE") != NULL);
}

static void test_opinions_supersede_sql_valid(void) {
    char buf[256];
    size_t len = 0;
    hu_error_t err = hu_opinions_supersede_sql(1, 2, buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(strstr(buf, "UPDATE") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "superseded_by") != NULL);
}

static void test_opinions_is_core_value_true(void) {
    const char *core_values[] = {"family", "honesty"};
    bool r = hu_opinions_is_core_value("family", 6, core_values, 2);
    HU_ASSERT_TRUE(r);
}

static void test_opinions_is_core_value_false(void) {
    const char *core_values[] = {"family", "honesty"};
    bool r = hu_opinions_is_core_value("pizza", 5, core_values, 2);
    HU_ASSERT_FALSE(r);
}

static void test_opinions_build_prompt_with_data(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_opinion_t ops[2] = {{0}};
    ops[0].topic = hu_strndup(&alloc, "pizza", 5);
    ops[0].topic_len = 5;
    ops[0].position = hu_strndup(&alloc, "delicious", 9);
    ops[0].position_len = 9;
    ops[0].confidence = 0.8;
    ops[1].topic = hu_strndup(&alloc, "coffee", 6);
    ops[1].topic_len = 6;
    ops[1].position = hu_strndup(&alloc, "essential", 9);
    ops[1].position_len = 9;
    ops[1].confidence = 0.9;

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_opinions_build_prompt(&alloc, ops, 2, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "pizza") != NULL);
    HU_ASSERT_TRUE(strstr(out, "coffee") != NULL);
    HU_ASSERT_TRUE(strstr(out, "[YOUR OPINIONS]") != NULL);

    hu_str_free(&alloc, out);
    hu_opinion_deinit(&alloc, &ops[0]);
    hu_opinion_deinit(&alloc, &ops[1]);
}

static void test_opinions_build_prompt_empty(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_opinions_build_prompt(&alloc, NULL, 0, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "No recorded opinions") != NULL);
    hu_str_free(&alloc, out);
}

static void test_chapters_create_table_valid(void) {
    char buf[1024];
    size_t len = 0;
    hu_error_t err = hu_chapters_create_table_sql(buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(strstr(buf, "CREATE TABLE") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "life_chapters") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "active") != NULL);
}

static void test_chapters_insert_sql_valid(void) {
    hu_life_chapter_t ch = {
        .theme = "settling into new job",
        .theme_len = 21,
        .mood = "optimistic",
        .mood_len = 10,
        .started_at = 1000u,
        .ended_at = 0u,
        .key_threads = "learning codebase",
        .key_threads_len = 17,
        .active = true,
    };
    char buf[512];
    size_t len = 0;
    hu_error_t err = hu_chapters_insert_sql(&ch, buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(strstr(buf, "INSERT INTO") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "settling") != NULL);
}

static void test_chapters_query_active_valid(void) {
    char buf[256];
    size_t len = 0;
    hu_error_t err = hu_chapters_query_active_sql(buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(strstr(buf, "WHERE active=1") != NULL);
}

static void test_chapters_close_sql_valid(void) {
    char buf[256];
    size_t len = 0;
    hu_error_t err = hu_chapters_close_sql(1, 2000u, buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(strstr(buf, "active=0") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "UPDATE") != NULL);
}

static void test_chapters_build_prompt_active(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_life_chapter_t ch = {0};
    ch.theme = hu_strndup(&alloc, "settling into new job", 21);
    ch.theme_len = 21;
    ch.mood = hu_strndup(&alloc, "optimistic", 9);
    ch.mood_len = 9;
    ch.key_threads = hu_strndup(&alloc, "learning codebase", 15);
    ch.key_threads_len = 15;

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_chapters_build_prompt(&alloc, &ch, 1, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "settling") != NULL);
    HU_ASSERT_TRUE(strstr(out, "[CURRENT LIFE CHAPTER]") != NULL);

    hu_str_free(&alloc, out);
    hu_chapter_deinit(&alloc, &ch);
}

static void test_chapters_build_prompt_none(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_chapters_build_prompt(&alloc, NULL, 0, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(strstr(out, "No active life chapter") != NULL);
    hu_str_free(&alloc, out);
}

static void test_social_graph_create_table_valid(void) {
    char buf[1024];
    size_t len = 0;
    hu_error_t err = hu_social_graph_create_table_sql(buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(strstr(buf, "CREATE TABLE") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "social_graph") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "PRIMARY KEY") != NULL);
}

static void test_social_graph_insert_link_valid(void) {
    hu_social_link_t link = {
        .contact_a = "alice",
        .contact_a_len = 5,
        .contact_b = "bob",
        .contact_b_len = 3,
        .rel_type = HU_SOCIAL_FRIEND,
        .closeness = 0.8,
        .context = "met at work",
        .context_len = 11,
    };
    char buf[512];
    size_t len = 0;
    hu_error_t err = hu_social_graph_insert_link_sql(&link, buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(strstr(buf, "INSERT OR REPLACE") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "alice") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "bob") != NULL);
}

static void test_social_graph_query_contact_valid(void) {
    char buf[512];
    size_t len = 0;
    hu_error_t err = hu_social_graph_query_for_contact_sql("alice", 5, buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(strstr(buf, "contact_a=") != NULL || strstr(buf, "contact_a='") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "OR contact_b=") != NULL || strstr(buf, "OR contact_b='") != NULL);
}

static void test_social_graph_contacts_connected_true(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_social_link_t links[1] = {{0}};
    links[0].contact_a = hu_strndup(&alloc, "alice", 5);
    links[0].contact_a_len = 5;
    links[0].contact_b = hu_strndup(&alloc, "bob", 3);
    links[0].contact_b_len = 3;

    bool r = hu_social_graph_contacts_connected(links, 1, "alice", 5, "bob", 3);
    HU_ASSERT_TRUE(r);
    bool r2 = hu_social_graph_contacts_connected(links, 1, "bob", 3, "alice", 5);
    HU_ASSERT_TRUE(r2);

    hu_social_link_deinit(&alloc, &links[0]);
}

static void test_social_graph_contacts_connected_false(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_social_link_t links[1] = {{0}};
    links[0].contact_a = hu_strndup(&alloc, "alice", 5);
    links[0].contact_a_len = 5;
    links[0].contact_b = hu_strndup(&alloc, "bob", 3);
    links[0].contact_b_len = 3;

    bool r = hu_social_graph_contacts_connected(links, 1, "alice", 5, "charlie", 7);
    HU_ASSERT_FALSE(r);

    hu_social_link_deinit(&alloc, &links[0]);
}

static void test_social_graph_build_prompt_data(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_social_link_t links[2] = {{0}};
    links[0].contact_a = hu_strndup(&alloc, "user", 4);
    links[0].contact_a_len = 4;
    links[0].contact_b = hu_strndup(&alloc, "Alice", 5);
    links[0].contact_b_len = 5;
    links[0].rel_type = HU_SOCIAL_FAMILY;
    links[1].contact_a = hu_strndup(&alloc, "user", 4);
    links[1].contact_a_len = 4;
    links[1].contact_b = hu_strndup(&alloc, "Bob", 3);
    links[1].contact_b_len = 3;
    links[1].rel_type = HU_SOCIAL_COWORKER;

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_social_graph_build_prompt(&alloc, links, 2, "user", 4, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "Alice") != NULL);
    HU_ASSERT_TRUE(strstr(out, "Bob") != NULL);
    HU_ASSERT_TRUE(strstr(out, "[SOCIAL NETWORK") != NULL);

    hu_str_free(&alloc, out);
    hu_social_link_deinit(&alloc, &links[0]);
    hu_social_link_deinit(&alloc, &links[1]);
}

static void test_deinit_frees_all(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_opinion_t op = {0};
    op.topic = hu_strndup(&alloc, "t", 1);
    op.topic_len = 1;
    op.position = hu_strndup(&alloc, "p", 1);
    op.position_len = 1;
    hu_opinion_deinit(&alloc, &op);
    HU_ASSERT_NULL(op.topic);
    HU_ASSERT_NULL(op.position);

    hu_life_chapter_t ch = {0};
    ch.theme = hu_strndup(&alloc, "t", 1);
    ch.theme_len = 1;
    ch.mood = hu_strndup(&alloc, "m", 1);
    ch.mood_len = 1;
    ch.key_threads = hu_strndup(&alloc, "k", 1);
    ch.key_threads_len = 1;
    hu_chapter_deinit(&alloc, &ch);
    HU_ASSERT_NULL(ch.theme);
    HU_ASSERT_NULL(ch.mood);
    HU_ASSERT_NULL(ch.key_threads);

    hu_social_link_t link = {0};
    link.contact_a = hu_strndup(&alloc, "a", 1);
    link.contact_a_len = 1;
    link.contact_b = hu_strndup(&alloc, "b", 1);
    link.contact_b_len = 1;
    link.context = hu_strndup(&alloc, "c", 1);
    link.context_len = 1;
    hu_social_link_deinit(&alloc, &link);
    HU_ASSERT_NULL(link.contact_a);
    HU_ASSERT_NULL(link.contact_b);
    HU_ASSERT_NULL(link.context);
}

void run_cognitive_tests(void) {
    HU_TEST_SUITE("cognitive");
    HU_RUN_TEST(test_opinions_create_table_valid);
    HU_RUN_TEST(test_opinions_upsert_sql_valid);
    HU_RUN_TEST(test_opinions_query_current_sql_valid);
    HU_RUN_TEST(test_opinions_supersede_sql_valid);
    HU_RUN_TEST(test_opinions_is_core_value_true);
    HU_RUN_TEST(test_opinions_is_core_value_false);
    HU_RUN_TEST(test_opinions_build_prompt_with_data);
    HU_RUN_TEST(test_opinions_build_prompt_empty);
    HU_RUN_TEST(test_chapters_create_table_valid);
    HU_RUN_TEST(test_chapters_insert_sql_valid);
    HU_RUN_TEST(test_chapters_query_active_valid);
    HU_RUN_TEST(test_chapters_close_sql_valid);
    HU_RUN_TEST(test_chapters_build_prompt_active);
    HU_RUN_TEST(test_chapters_build_prompt_none);
    HU_RUN_TEST(test_social_graph_create_table_valid);
    HU_RUN_TEST(test_social_graph_insert_link_valid);
    HU_RUN_TEST(test_social_graph_query_contact_valid);
    HU_RUN_TEST(test_social_graph_contacts_connected_true);
    HU_RUN_TEST(test_social_graph_contacts_connected_false);
    HU_RUN_TEST(test_social_graph_build_prompt_data);
    HU_RUN_TEST(test_deinit_frees_all);
}
