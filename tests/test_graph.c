/*
 * GraphRAG (sc_graph_t) tests.
 * Requires SC_ENABLE_SQLITE. Uses :memory: when SC_IS_TEST (test build).
 */
#include "seaclaw/core/allocator.h"
#include "seaclaw/memory/graph.h"
#include "test_framework.h"
#include <stdio.h>
#include <string.h>

#ifdef SC_ENABLE_SQLITE

static void graph_open_valid_path_succeeds(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_error_t err = sc_graph_open(&alloc, "ignored", 7, &g);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(g);
    sc_graph_close(g, &alloc);
}

static void graph_open_null_alloc_returns_error(void) {
    sc_graph_t *g = NULL;
    sc_error_t err = sc_graph_open(NULL, "path", 4, &g);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
    SC_ASSERT_NULL(g);
}

static void graph_open_null_out_returns_error(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_error_t err = sc_graph_open(&alloc, "path", 4, NULL);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
}

static void graph_close_null_graph_no_op(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_close(NULL, &alloc);
}

static void graph_close_null_alloc_no_op(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);
    SC_ASSERT_NOT_NULL(g);
    sc_graph_close(g, NULL);
    sc_graph_close(g, &alloc);
}

static void graph_close_valid_releases(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);
    sc_graph_close(g, &alloc);
}

static void graph_upsert_entity_insert_new_succeeds(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);

    int64_t id = 0;
    sc_error_t err =
        sc_graph_upsert_entity(g, "alice", 5, SC_ENTITY_PERSON, NULL, &id);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_TRUE(id > 0);

    sc_graph_close(g, &alloc);
}

static void graph_upsert_entity_update_existing_succeeds(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);

    int64_t id1 = 0;
    sc_graph_upsert_entity(g, "bob", 3, SC_ENTITY_PERSON, NULL, &id1);

    int64_t id2 = 0;
    sc_error_t err =
        sc_graph_upsert_entity(g, "bob", 3, SC_ENTITY_PERSON, NULL, &id2);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(id1, id2);

    sc_graph_close(g, &alloc);
}

static void graph_upsert_entity_null_name_returns_error(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);

    int64_t id = 0;
    sc_error_t err =
        sc_graph_upsert_entity(g, NULL, 5, SC_ENTITY_PERSON, NULL, &id);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);

    sc_graph_close(g, &alloc);
}

static void graph_upsert_entity_empty_name_returns_error(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);

    int64_t id = 0;
    sc_error_t err =
        sc_graph_upsert_entity(g, "alice", 0, SC_ENTITY_PERSON, NULL, &id);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);

    sc_graph_close(g, &alloc);
}

static void graph_upsert_entity_null_out_id_returns_error(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);

    sc_error_t err =
        sc_graph_upsert_entity(g, "alice", 5, SC_ENTITY_PERSON, NULL, NULL);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);

    sc_graph_close(g, &alloc);
}

static void graph_upsert_entity_with_metadata(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);

    int64_t id = 0;
    const char *meta = "{\"role\":\"friend\"}";
    sc_error_t err = sc_graph_upsert_entity(g, "charlie", 7, SC_ENTITY_PERSON,
                                            meta, &id);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_TRUE(id > 0);

    sc_graph_entity_t ent;
    err = sc_graph_find_entity(g, "charlie", 7, &ent);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(ent.metadata_json);
    SC_ASSERT_TRUE(strstr(ent.metadata_json, "friend") != NULL);

    if (ent.name)
        alloc.free(alloc.ctx, ent.name, ent.name_len + 1);
    if (ent.metadata_json)
        alloc.free(alloc.ctx, ent.metadata_json, strlen(ent.metadata_json) + 1);
    sc_graph_close(g, &alloc);
}

static void graph_find_entity_existing_returns_data(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);

    int64_t id = 0;
    sc_graph_upsert_entity(g, "dave", 4, SC_ENTITY_ORGANIZATION, NULL, &id);

    sc_graph_entity_t ent;
    sc_error_t err = sc_graph_find_entity(g, "dave", 4, &ent);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(ent.id, id);
    SC_ASSERT_NOT_NULL(ent.name);
    SC_ASSERT_EQ(ent.name_len, 4u);
    SC_ASSERT_EQ(ent.type, SC_ENTITY_ORGANIZATION);
    SC_ASSERT_TRUE(ent.mention_count >= 1);

    if (ent.name)
        alloc.free(alloc.ctx, ent.name, ent.name_len + 1);
    if (ent.metadata_json)
        alloc.free(alloc.ctx, ent.metadata_json, strlen(ent.metadata_json) + 1);
    sc_graph_close(g, &alloc);
}

static void graph_find_entity_nonexistent_returns_not_found(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);

    sc_graph_entity_t ent;
    sc_error_t err = sc_graph_find_entity(g, "never_inserted", 14, &ent);
    SC_ASSERT_EQ(err, SC_ERR_NOT_FOUND);

    sc_graph_close(g, &alloc);
}

static void graph_find_entity_null_name_returns_error(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);

    sc_graph_entity_t ent;
    sc_error_t err = sc_graph_find_entity(g, NULL, 5, &ent);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);

    sc_graph_close(g, &alloc);
}

static void graph_find_entity_null_out_returns_error(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);
    int64_t id = 0;
    sc_graph_upsert_entity(g, "eve", 3, SC_ENTITY_PERSON, NULL, &id);

    sc_error_t err = sc_graph_find_entity(g, "eve", 3, NULL);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);

    sc_graph_close(g, &alloc);
}

static void graph_upsert_relation_insert_new_succeeds(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);

    int64_t a = 0, b = 0;
    sc_graph_upsert_entity(g, "alice", 5, SC_ENTITY_PERSON, NULL, &a);
    sc_graph_upsert_entity(g, "bob", 3, SC_ENTITY_PERSON, NULL, &b);

    sc_error_t err = sc_graph_upsert_relation(g, a, b, SC_REL_KNOWS, 1.0f,
                                              "met at work", 12);
    SC_ASSERT_EQ(err, SC_OK);

    sc_graph_close(g, &alloc);
}

static void graph_upsert_relation_update_weight_succeeds(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);

    int64_t a = 0, b = 0;
    sc_graph_upsert_entity(g, "x", 1, SC_ENTITY_PERSON, NULL, &a);
    sc_graph_upsert_entity(g, "y", 1, SC_ENTITY_PERSON, NULL, &b);

    sc_graph_upsert_relation(g, a, b, SC_REL_WORKS_AT, 0.5f, NULL, 0);
    sc_error_t err =
        sc_graph_upsert_relation(g, a, b, SC_REL_WORKS_AT, 0.9f, "updated", 7);
    SC_ASSERT_EQ(err, SC_OK);

    sc_graph_close(g, &alloc);
}

static void graph_upsert_relation_invalid_ids_returns_io(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);

    sc_error_t err =
        sc_graph_upsert_relation(g, 99999, 99998, SC_REL_KNOWS, 1.0f, NULL, 0);
    SC_ASSERT_EQ(err, SC_ERR_IO);

    sc_graph_close(g, &alloc);
}

static void graph_neighbors_returns_adjacent_entities(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);

    int64_t a = 0, b = 0, c = 0;
    sc_graph_upsert_entity(g, "alice", 5, SC_ENTITY_PERSON, NULL, &a);
    sc_graph_upsert_entity(g, "bob", 3, SC_ENTITY_PERSON, NULL, &b);
    sc_graph_upsert_entity(g, "charlie", 7, SC_ENTITY_PERSON, NULL, &c);
    sc_graph_upsert_relation(g, a, b, SC_REL_KNOWS, 1.0f, NULL, 0);
    sc_graph_upsert_relation(g, a, c, SC_REL_KNOWS, 1.0f, NULL, 0);

    sc_graph_entity_t *entities = NULL;
    sc_graph_relation_t *relations = NULL;
    size_t count = 0;
    sc_error_t err =
        sc_graph_neighbors(g, &alloc, a, 1, 10, &entities, &relations, &count);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(count, 2u);
    SC_ASSERT_NOT_NULL(entities);
    SC_ASSERT_NOT_NULL(relations);

    sc_graph_entities_free(&alloc, entities, count);
    sc_graph_relations_free(&alloc, relations, count);
    sc_graph_close(g, &alloc);
}

static void graph_neighbors_no_neighbors_returns_empty(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);

    int64_t a = 0;
    sc_graph_upsert_entity(g, "isolated", 8, SC_ENTITY_TOPIC, NULL, &a);

    sc_graph_entity_t *entities = NULL;
    sc_graph_relation_t *relations = NULL;
    size_t count = 0;
    sc_error_t err =
        sc_graph_neighbors(g, &alloc, a, 1, 10, &entities, &relations, &count);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(count, 0u);
    SC_ASSERT_NULL(entities);
    SC_ASSERT_NULL(relations);

    sc_graph_close(g, &alloc);
}

static void graph_neighbors_invalid_entity_returns_empty(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);

    sc_graph_entity_t *entities = NULL;
    sc_graph_relation_t *relations = NULL;
    size_t count = 0;
    sc_error_t err = sc_graph_neighbors(g, &alloc, 99999, 1, 10, &entities,
                                        &relations, &count);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(count, 0u);

    sc_graph_close(g, &alloc);
}

static void graph_neighbors_null_params_returns_error(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);

    int64_t a = 0;
    sc_graph_upsert_entity(g, "a", 1, SC_ENTITY_PERSON, NULL, &a);

    sc_graph_entity_t *entities = NULL;
    sc_graph_relation_t *relations = NULL;
    size_t count = 0;

    sc_error_t err =
        sc_graph_neighbors(NULL, &alloc, a, 1, 10, &entities, &relations, &count);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);

    err = sc_graph_neighbors(g, NULL, a, 1, 10, &entities, &relations, &count);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);

    sc_graph_close(g, &alloc);
}

static void graph_build_context_with_data_returns_formatted(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);

    int64_t a = 0, b = 0;
    sc_graph_upsert_entity(g, "alice", 5, SC_ENTITY_PERSON, NULL, &a);
    sc_graph_upsert_entity(g, "bob", 3, SC_ENTITY_PERSON, NULL, &b);
    sc_graph_upsert_relation(g, a, b, SC_REL_KNOWS, 1.0f, NULL, 0);

    char *out = NULL;
    size_t out_len = 0;
    sc_error_t err =
        sc_graph_build_context(g, &alloc, "alice bob", 9, 1, 4096, &out, &out_len);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(out);
    SC_ASSERT_TRUE(out_len > 0);
    SC_ASSERT_TRUE(strstr(out, "Knowledge Graph") != NULL);
    SC_ASSERT_TRUE(strstr(out, "alice") != NULL || strstr(out, "bob") != NULL);

    alloc.free(alloc.ctx, out, out_len + 1);
    sc_graph_close(g, &alloc);
}

static void graph_build_context_empty_graph_returns_empty_string(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);

    char *out = NULL;
    size_t out_len = 0;
    sc_error_t err =
        sc_graph_build_context(g, &alloc, "nonexistent", 11, 1, 4096, &out,
                               &out_len);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(out);
    SC_ASSERT_EQ(out_len, 0u);

    alloc.free(alloc.ctx, out, out_len + 1);
    sc_graph_close(g, &alloc);
}

static void graph_build_context_null_query_returns_error(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);

    char *out = NULL;
    size_t out_len = 0;
    sc_error_t err =
        sc_graph_build_context(g, &alloc, NULL, 5, 1, 4096, &out, &out_len);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);

    sc_graph_close(g, &alloc);
}

static void graph_build_context_null_out_returns_error(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);

    size_t out_len = 0;
    sc_error_t err =
        sc_graph_build_context(g, &alloc, "query", 5, 1, 4096, NULL, &out_len);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);

    sc_graph_close(g, &alloc);
}

static void graph_build_contact_context_with_contact_prepends_header(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);

    int64_t a = 0, b = 0;
    sc_graph_upsert_entity(g, "alice", 5, SC_ENTITY_PERSON, NULL, &a);
    sc_graph_upsert_entity(g, "bob", 3, SC_ENTITY_PERSON, NULL, &b);
    sc_graph_upsert_relation(g, a, b, SC_REL_KNOWS, 1.0f, NULL, 0);

    char *out = NULL;
    size_t out_len = 0;
    sc_error_t err = sc_graph_build_contact_context(
        g, &alloc, "alice", 5, "contact_123", 10, 1, 4096, &out, &out_len);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(out);
    SC_ASSERT_TRUE(out_len > 0);
    SC_ASSERT_TRUE(strstr(out, "Knowledge relevant to this contact") != NULL);

    alloc.free(alloc.ctx, out, out_len + 1);
    sc_graph_close(g, &alloc);
}

static void graph_build_contact_context_empty_contact_returns_plain_context(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);

    int64_t a = 0;
    sc_graph_upsert_entity(g, "alice", 5, SC_ENTITY_PERSON, NULL, &a);

    char *out = NULL;
    size_t out_len = 0;
    sc_error_t err = sc_graph_build_contact_context(
        g, &alloc, "alice", 5, NULL, 0, 1, 4096, &out, &out_len);
    SC_ASSERT_EQ(err, SC_OK);

    if (out)
        alloc.free(alloc.ctx, out, out_len + 1);
    sc_graph_close(g, &alloc);
}

static void graph_build_communities_with_data_returns_clusters(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);

    int64_t a = 0, b = 0, c = 0;
    sc_graph_upsert_entity(g, "alice", 5, SC_ENTITY_PERSON, NULL, &a);
    sc_graph_upsert_entity(g, "bob", 3, SC_ENTITY_PERSON, NULL, &b);
    sc_graph_upsert_entity(g, "charlie", 7, SC_ENTITY_PERSON, NULL, &c);
    sc_graph_upsert_relation(g, a, b, SC_REL_KNOWS, 1.0f, NULL, 0);
    sc_graph_upsert_relation(g, b, c, SC_REL_KNOWS, 1.0f, NULL, 0);

    char *out = NULL;
    size_t out_len = 0;
    sc_error_t err =
        sc_graph_build_communities(g, &alloc, 10, 4096, &out, &out_len);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(out);
    SC_ASSERT_TRUE(out_len > 0);
    SC_ASSERT_TRUE(strstr(out, "Topic Clusters") != NULL);

    alloc.free(alloc.ctx, out, out_len + 1);
    sc_graph_close(g, &alloc);
}

static void graph_build_communities_empty_graph_returns_header_only(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);

    char *out = NULL;
    size_t out_len = 0;
    sc_error_t err =
        sc_graph_build_communities(g, &alloc, 10, 4096, &out, &out_len);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(out);
    SC_ASSERT_TRUE(strstr(out, "Topic Clusters") != NULL);

    alloc.free(alloc.ctx, out, out_len + 1);
    sc_graph_close(g, &alloc);
}

static void graph_entities_free_null_alloc_no_op(void) {
    sc_graph_entities_free(NULL, (sc_graph_entity_t *)0x1, 1);
}

static void graph_entities_free_null_entities_no_op(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_entities_free(&alloc, NULL, 0);
}

static void graph_relations_free_null_alloc_no_op(void) {
    sc_graph_relations_free(NULL, (sc_graph_relation_t *)0x1, 1);
}

static void graph_relations_free_null_relations_no_op(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_relations_free(&alloc, NULL, 0);
}

static void entity_type_from_string_all_types(void) {
    SC_ASSERT_EQ(sc_entity_type_from_string("person", 6), SC_ENTITY_PERSON);
    SC_ASSERT_EQ(sc_entity_type_from_string("place", 5), SC_ENTITY_PLACE);
    SC_ASSERT_EQ(sc_entity_type_from_string("organization", 12),
                SC_ENTITY_ORGANIZATION);
    SC_ASSERT_EQ(sc_entity_type_from_string("event", 5), SC_ENTITY_EVENT);
    SC_ASSERT_EQ(sc_entity_type_from_string("topic", 5), SC_ENTITY_TOPIC);
    SC_ASSERT_EQ(sc_entity_type_from_string("emotion", 7), SC_ENTITY_EMOTION);
}

static void entity_type_from_string_null_returns_unknown(void) {
    SC_ASSERT_EQ(sc_entity_type_from_string(NULL, 5), SC_ENTITY_UNKNOWN);
}

static void entity_type_from_string_empty_returns_unknown(void) {
    SC_ASSERT_EQ(sc_entity_type_from_string("person", 0), SC_ENTITY_UNKNOWN);
}

static void entity_type_from_string_case_insensitive(void) {
    SC_ASSERT_EQ(sc_entity_type_from_string("PERSON", 6), SC_ENTITY_PERSON);
    SC_ASSERT_EQ(sc_entity_type_from_string("Place", 5), SC_ENTITY_PLACE);
}

static void entity_type_to_string_all_types(void) {
    SC_ASSERT_STR_EQ(sc_entity_type_to_string(SC_ENTITY_PERSON), "person");
    SC_ASSERT_STR_EQ(sc_entity_type_to_string(SC_ENTITY_PLACE), "place");
    SC_ASSERT_STR_EQ(sc_entity_type_to_string(SC_ENTITY_ORGANIZATION),
                     "organization");
    SC_ASSERT_STR_EQ(sc_entity_type_to_string(SC_ENTITY_EVENT), "event");
    SC_ASSERT_STR_EQ(sc_entity_type_to_string(SC_ENTITY_TOPIC), "topic");
    SC_ASSERT_STR_EQ(sc_entity_type_to_string(SC_ENTITY_EMOTION), "emotion");
    SC_ASSERT_STR_EQ(sc_entity_type_to_string(SC_ENTITY_UNKNOWN), "unknown");
}

static void relation_type_from_string_all_types(void) {
    SC_ASSERT_EQ(sc_relation_type_from_string("knows", strlen("knows")),
                 SC_REL_KNOWS);
    SC_ASSERT_EQ(sc_relation_type_from_string("family_of", strlen("family_of")),
                 SC_REL_FAMILY_OF);
    SC_ASSERT_EQ(sc_relation_type_from_string("works_at", strlen("works_at")),
                 SC_REL_WORKS_AT);
    SC_ASSERT_EQ(sc_relation_type_from_string("lives_in", strlen("lives_in")),
                 SC_REL_LIVES_IN);
    SC_ASSERT_EQ(sc_relation_type_from_string("interested_in",
                                              strlen("interested_in")),
                 SC_REL_INTERESTED_IN);
    SC_ASSERT_EQ(sc_relation_type_from_string("discussed_with",
                                              strlen("discussed_with")),
                 SC_REL_DISCUSSED_WITH);
    SC_ASSERT_EQ(sc_relation_type_from_string("feels_about", strlen("feels_about")),
                 SC_REL_FEELS_ABOUT);
    SC_ASSERT_EQ(sc_relation_type_from_string("promised_to", strlen("promised_to")),
                 SC_REL_PROMISED_TO);
    SC_ASSERT_EQ(sc_relation_type_from_string("shared_experience",
                                              strlen("shared_experience")),
                 SC_REL_SHARED_EXPERIENCE);
    SC_ASSERT_EQ(sc_relation_type_from_string("related_to", strlen("related_to")),
                 SC_REL_RELATED_TO);
}

static void relation_type_from_string_aliases(void) {
    SC_ASSERT_EQ(sc_relation_type_from_string("family", 6), SC_REL_FAMILY_OF);
    SC_ASSERT_EQ(sc_relation_type_from_string("job", 3), SC_REL_WORKS_AT);
    SC_ASSERT_EQ(sc_relation_type_from_string("likes", 5), SC_REL_INTERESTED_IN);
    SC_ASSERT_EQ(sc_relation_type_from_string("is_a", 4), SC_REL_RELATED_TO);
}

static void relation_type_from_string_null_returns_related_to(void) {
    SC_ASSERT_EQ(sc_relation_type_from_string(NULL, 5), SC_REL_RELATED_TO);
}

static void relation_type_to_string_all_types(void) {
    SC_ASSERT_STR_EQ(sc_relation_type_to_string(SC_REL_KNOWS), "knows");
    SC_ASSERT_STR_EQ(sc_relation_type_to_string(SC_REL_FAMILY_OF), "family_of");
    SC_ASSERT_STR_EQ(sc_relation_type_to_string(SC_REL_WORKS_AT), "works_at");
    SC_ASSERT_STR_EQ(sc_relation_type_to_string(SC_REL_LIVES_IN), "lives_in");
    SC_ASSERT_STR_EQ(sc_relation_type_to_string(SC_REL_INTERESTED_IN),
                     "interested_in");
    SC_ASSERT_STR_EQ(sc_relation_type_to_string(SC_REL_DISCUSSED_WITH),
                     "discussed_with");
    SC_ASSERT_STR_EQ(sc_relation_type_to_string(SC_REL_FEELS_ABOUT),
                     "feels_about");
    SC_ASSERT_STR_EQ(sc_relation_type_to_string(SC_REL_PROMISED_TO),
                     "promised_to");
    SC_ASSERT_STR_EQ(sc_relation_type_to_string(SC_REL_SHARED_EXPERIENCE),
                     "shared_experience");
    SC_ASSERT_STR_EQ(sc_relation_type_to_string(SC_REL_RELATED_TO),
                     "related_to");
}

static void graph_entity_special_chars_in_name(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);

    const char *name = "O'Brien";
    int64_t id = 0;
    sc_error_t err =
        sc_graph_upsert_entity(g, name, 7, SC_ENTITY_PERSON, NULL, &id);
    SC_ASSERT_EQ(err, SC_OK);

    sc_graph_entity_t ent;
    err = sc_graph_find_entity(g, name, 7, &ent);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(ent.name);
    SC_ASSERT_EQ(ent.name_len, 7u);

    if (ent.name)
        alloc.free(alloc.ctx, ent.name, ent.name_len + 1);
    if (ent.metadata_json)
        alloc.free(alloc.ctx, ent.metadata_json, strlen(ent.metadata_json) + 1);
    sc_graph_close(g, &alloc);
}

static void graph_entity_unicode_in_name(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);

    const char *name = "Jos\xc3\xa9";
    size_t len = 4;
    int64_t id = 0;
    sc_error_t err =
        sc_graph_upsert_entity(g, name, len, SC_ENTITY_PERSON, NULL, &id);
    SC_ASSERT_EQ(err, SC_OK);

    sc_graph_entity_t ent;
    err = sc_graph_find_entity(g, name, len, &ent);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(ent.name);

    if (ent.name)
        alloc.free(alloc.ctx, ent.name, ent.name_len + 1);
    if (ent.metadata_json)
        alloc.free(alloc.ctx, ent.metadata_json, strlen(ent.metadata_json) + 1);
    sc_graph_close(g, &alloc);
}

static void graph_neighbors_max_results_respected(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);

    int64_t center = 0;
    sc_graph_upsert_entity(g, "center", 6, SC_ENTITY_PERSON, NULL, &center);
    for (int i = 0; i < 5; i++) {
        char name[16];
        int n = snprintf(name, sizeof(name), "n%d", i);
        int64_t id = 0;
        sc_graph_upsert_entity(g, name, (size_t)n, SC_ENTITY_PERSON, NULL, &id);
        sc_graph_upsert_relation(g, center, id, SC_REL_KNOWS, 1.0f, NULL, 0);
    }

    sc_graph_entity_t *entities = NULL;
    sc_graph_relation_t *relations = NULL;
    size_t count = 0;
    sc_error_t err = sc_graph_neighbors(g, &alloc, center, 1, 2, &entities,
                                        &relations, &count);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(count, 2u);

    sc_graph_entities_free(&alloc, entities, count);
    sc_graph_relations_free(&alloc, relations, count);
    sc_graph_close(g, &alloc);
}

static void graph_upsert_entity_null_graph_returns_error(void) {
    int64_t id = 0;
    sc_error_t err =
        sc_graph_upsert_entity(NULL, "alice", 5, SC_ENTITY_PERSON, NULL, &id);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
}

static void graph_find_entity_null_graph_returns_error(void) {
    sc_graph_entity_t ent;
    sc_error_t err = sc_graph_find_entity(NULL, "alice", 5, &ent);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
}

static void graph_upsert_relation_null_graph_returns_error(void) {
    sc_error_t err =
        sc_graph_upsert_relation(NULL, 1, 2, SC_REL_KNOWS, 1.0f, NULL, 0);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
}

static void graph_build_context_null_graph_returns_error(void) {
    sc_allocator_t alloc = sc_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    sc_error_t err =
        sc_graph_build_context(NULL, &alloc, "q", 1, 1, 4096, &out, &out_len);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
    SC_ASSERT_NULL(out);
}

static void graph_build_communities_null_params_returns_error(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_graph_t *g = NULL;
    sc_graph_open(&alloc, "x", 1, &g);

    char *out = NULL;
    size_t out_len = 0;
    sc_error_t err =
        sc_graph_build_communities(NULL, &alloc, 10, 4096, &out, &out_len);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);

    sc_graph_close(g, &alloc);
}

void run_graph_tests(void) {
    SC_TEST_SUITE("graph");
    SC_RUN_TEST(graph_open_valid_path_succeeds);
    SC_RUN_TEST(graph_open_null_alloc_returns_error);
    SC_RUN_TEST(graph_open_null_out_returns_error);
    SC_RUN_TEST(graph_close_null_graph_no_op);
    SC_RUN_TEST(graph_close_null_alloc_no_op);
    SC_RUN_TEST(graph_close_valid_releases);
    SC_RUN_TEST(graph_upsert_entity_insert_new_succeeds);
    SC_RUN_TEST(graph_upsert_entity_update_existing_succeeds);
    SC_RUN_TEST(graph_upsert_entity_null_name_returns_error);
    SC_RUN_TEST(graph_upsert_entity_empty_name_returns_error);
    SC_RUN_TEST(graph_upsert_entity_null_out_id_returns_error);
    SC_RUN_TEST(graph_upsert_entity_with_metadata);
    SC_RUN_TEST(graph_upsert_entity_null_graph_returns_error);
    SC_RUN_TEST(graph_find_entity_existing_returns_data);
    SC_RUN_TEST(graph_find_entity_nonexistent_returns_not_found);
    SC_RUN_TEST(graph_find_entity_null_name_returns_error);
    SC_RUN_TEST(graph_find_entity_null_out_returns_error);
    SC_RUN_TEST(graph_find_entity_null_graph_returns_error);
    SC_RUN_TEST(graph_upsert_relation_insert_new_succeeds);
    SC_RUN_TEST(graph_upsert_relation_update_weight_succeeds);
    SC_RUN_TEST(graph_upsert_relation_invalid_ids_returns_io);
    SC_RUN_TEST(graph_upsert_relation_null_graph_returns_error);
    SC_RUN_TEST(graph_neighbors_returns_adjacent_entities);
    SC_RUN_TEST(graph_neighbors_no_neighbors_returns_empty);
    SC_RUN_TEST(graph_neighbors_invalid_entity_returns_empty);
    SC_RUN_TEST(graph_neighbors_null_params_returns_error);
    SC_RUN_TEST(graph_neighbors_max_results_respected);
    SC_RUN_TEST(graph_build_context_with_data_returns_formatted);
    SC_RUN_TEST(graph_build_context_empty_graph_returns_empty_string);
    SC_RUN_TEST(graph_build_context_null_query_returns_error);
    SC_RUN_TEST(graph_build_context_null_out_returns_error);
    SC_RUN_TEST(graph_build_context_null_graph_returns_error);
    SC_RUN_TEST(graph_build_contact_context_with_contact_prepends_header);
    SC_RUN_TEST(graph_build_contact_context_empty_contact_returns_plain_context);
    SC_RUN_TEST(graph_build_communities_with_data_returns_clusters);
    SC_RUN_TEST(graph_build_communities_empty_graph_returns_header_only);
    SC_RUN_TEST(graph_build_communities_null_params_returns_error);
    SC_RUN_TEST(graph_entities_free_null_alloc_no_op);
    SC_RUN_TEST(graph_entities_free_null_entities_no_op);
    SC_RUN_TEST(graph_relations_free_null_alloc_no_op);
    SC_RUN_TEST(graph_relations_free_null_relations_no_op);
    SC_RUN_TEST(entity_type_from_string_all_types);
    SC_RUN_TEST(entity_type_from_string_null_returns_unknown);
    SC_RUN_TEST(entity_type_from_string_empty_returns_unknown);
    SC_RUN_TEST(entity_type_from_string_case_insensitive);
    SC_RUN_TEST(entity_type_to_string_all_types);
    SC_RUN_TEST(relation_type_from_string_all_types);
    SC_RUN_TEST(relation_type_from_string_aliases);
    SC_RUN_TEST(relation_type_from_string_null_returns_related_to);
    SC_RUN_TEST(relation_type_to_string_all_types);
    SC_RUN_TEST(graph_entity_special_chars_in_name);
    SC_RUN_TEST(graph_entity_unicode_in_name);
}

#else

void run_graph_tests(void) {
    SC_TEST_SUITE("graph");
}

#endif /* SC_ENABLE_SQLITE */
