#include "test_framework.h"
#include "human/tools/computer_use.h"

static void test_cu_create(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = hu_computer_use_create(&alloc);
    HU_ASSERT(tool.vtable != NULL);
    HU_ASSERT_STR_EQ(tool.vtable->name(tool.ctx), "computer_use");
}

static void test_cu_execute(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = hu_computer_use_create(&alloc);
    hu_json_value_t args; memset(&args, 0, sizeof(args));
    hu_tool_result_t result; memset(&result, 0, sizeof(result));
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, &args, &result), HU_OK);
    HU_ASSERT(result.success);
}

void run_computer_use_tests(void) {
    HU_TEST_SUITE("Computer Use Tool");
    HU_RUN_TEST(test_cu_create);
    HU_RUN_TEST(test_cu_execute);
}
