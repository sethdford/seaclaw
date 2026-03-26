#include "human/agent/hula.h"
#include "human/core/allocator.h"
#include "human/core/string.h"
#include "test_framework.h"
#include <string.h>

static void hula_golden_parse_minimal_program(void) {
    hu_allocator_t alloc = hu_system_allocator();
    static const char json[] =
        "{\"name\":\"golden_min\",\"version\":1,\"root\":"
        "{\"op\":\"call\",\"id\":\"c\",\"tool\":\"echo\",\"args\":{\"text\":\"hi\"}}}";

    hu_hula_program_t prog;
    hu_error_t err = hu_hula_parse_json(&alloc, json, strlen(json), &prog);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(prog.root);
    HU_ASSERT_STR_EQ(prog.name, "golden_min");
    hu_hula_program_deinit(&prog);
}

static void hula_golden_parse_with_sequence(void) {
    hu_allocator_t alloc = hu_system_allocator();
    static const char json[] =
        "{\"name\":\"golden_seq\",\"version\":1,\"root\":"
        "{\"op\":\"seq\",\"id\":\"s\",\"children\":["
        "{\"op\":\"call\",\"id\":\"c1\",\"tool\":\"echo\",\"args\":{\"text\":\"a\"}},"
        "{\"op\":\"call\",\"id\":\"c2\",\"tool\":\"echo\",\"args\":{\"text\":\"b\"}}"
        "]}}";
    hu_hula_program_t prog;
    hu_error_t err = hu_hula_parse_json(&alloc, json, strlen(json), &prog);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(prog.root);
    HU_ASSERT_STR_EQ(prog.name, "golden_seq");
    HU_ASSERT_EQ(prog.root->op, HU_HULA_SEQ);
    HU_ASSERT_EQ(prog.root->children_count, 2u);
    hu_hula_program_deinit(&prog);
}

static void hula_golden_parse_empty_name_ok(void) {
    hu_allocator_t alloc = hu_system_allocator();
    static const char json[] = "{\"name\":\"\",\"version\":1,\"root\":"
                               "{\"op\":\"call\",\"id\":\"c\",\"tool\":\"nop\",\"args\":{}}}";
    hu_hula_program_t prog;
    hu_error_t err = hu_hula_parse_json(&alloc, json, strlen(json), &prog);
    HU_ASSERT_EQ(err, HU_OK);
    hu_hula_program_deinit(&prog);
}

static void hula_golden_parse_invalid_json_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_hula_program_t prog;
    hu_error_t err = hu_hula_parse_json(&alloc, "{bad", 4, &prog);
    HU_ASSERT_TRUE(err != HU_OK);
}

static void hula_golden_roundtrip_serialize(void) {
    hu_allocator_t alloc = hu_system_allocator();
    static const char json[] =
        "{\"name\":\"rt\",\"version\":1,\"root\":"
        "{\"op\":\"call\",\"id\":\"c1\",\"tool\":\"echo\",\"args\":{\"text\":\"hello\"}}}";
    hu_hula_program_t prog;
    HU_ASSERT_EQ(hu_hula_parse_json(&alloc, json, strlen(json), &prog), HU_OK);
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(hu_hula_to_json(&alloc, &prog, &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(out_len > 0);
    HU_ASSERT_TRUE(strstr(out, "echo") != NULL);
    alloc.free(alloc.ctx, out, out_len);
    hu_hula_program_deinit(&prog);
}

void run_hula_golden_tests(void) {
    HU_TEST_SUITE("hula_golden");
    HU_RUN_TEST(hula_golden_parse_minimal_program);
    HU_RUN_TEST(hula_golden_parse_with_sequence);
    HU_RUN_TEST(hula_golden_parse_empty_name_ok);
    HU_RUN_TEST(hula_golden_parse_invalid_json_fails);
    HU_RUN_TEST(hula_golden_roundtrip_serialize);
}
