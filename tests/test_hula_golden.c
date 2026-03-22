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

void run_hula_golden_tests(void) {
    HU_TEST_SUITE("hula_golden");
    HU_RUN_TEST(hula_golden_parse_minimal_program);
}
