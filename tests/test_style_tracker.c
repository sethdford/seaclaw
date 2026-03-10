#ifdef HU_ENABLE_SQLITE

#include "human/context/style_tracker.h"
#include "human/core/allocator.h"
#include "human/memory.h"
#include "test_framework.h"
#include <string.h>

static void style_update_haha_sets_laugh_style_and_lowercase(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.ctx);

    hu_error_t err =
        hu_style_fingerprint_update(&mem, &alloc, "contact_x", 9, "haha that's great", 17);
    HU_ASSERT_EQ(err, HU_OK);

    hu_style_fingerprint_t fp;
    memset(&fp, 0, sizeof(fp));
    err = hu_style_fingerprint_get(&mem, &alloc, "contact_x", 9, &fp);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(fp.uses_lowercase);
    HU_ASSERT_STR_EQ(fp.laugh_style, "haha");

    mem.vtable->deinit(mem.ctx);
}

static void style_update_periods_sets_uses_periods(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.ctx);

    hu_error_t err =
        hu_style_fingerprint_update(&mem, &alloc, "contact_y", 9, "OK. Sounds good.", 16);
    HU_ASSERT_EQ(err, HU_OK);

    hu_style_fingerprint_t fp;
    memset(&fp, 0, sizeof(fp));
    err = hu_style_fingerprint_get(&mem, &alloc, "contact_y", 9, &fp);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(fp.uses_periods);
    HU_ASSERT_FALSE(fp.uses_lowercase);

    mem.vtable->deinit(mem.ctx);
}

static void style_get_build_directive_contains_hints(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.ctx);

    (void)hu_style_fingerprint_update(&mem, &alloc, "contact_z", 9, "haha that's great", 17);

    hu_style_fingerprint_t fp;
    memset(&fp, 0, sizeof(fp));
    hu_style_fingerprint_get(&mem, &alloc, "contact_z", 9, &fp);

    char buf[256];
    size_t len = hu_style_fingerprint_build_directive(&fp, buf, sizeof(buf));
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_TRUE(strstr(buf, "STYLE") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "haha") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "Match it") != NULL);

    mem.vtable->deinit(mem.ctx);
}

static void style_no_fingerprint_build_directive_returns_zero(void) {
    hu_style_fingerprint_t fp;
    memset(&fp, 0, sizeof(fp));

    char buf[256];
    size_t len = hu_style_fingerprint_build_directive(&fp, buf, sizeof(buf));
    HU_ASSERT_EQ(len, 0u);
}

static void style_get_nonexistent_returns_zeroed(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.ctx);

    hu_style_fingerprint_t fp;
    memset(&fp, 0xff, sizeof(fp));
    hu_error_t err = hu_style_fingerprint_get(&mem, &alloc, "nonexistent", 11, &fp);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_FALSE(fp.uses_lowercase);
    HU_ASSERT_FALSE(fp.uses_periods);
    HU_ASSERT_EQ(fp.laugh_style[0], '\0');

    char buf[256];
    size_t len = hu_style_fingerprint_build_directive(&fp, buf, sizeof(buf));
    HU_ASSERT_EQ(len, 0u);

    mem.vtable->deinit(mem.ctx);
}

static void style_none_memory_returns_not_supported(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_none_memory_create(&alloc);

    hu_error_t err =
        hu_style_fingerprint_update(&mem, &alloc, "x", 1, "haha", 4);
    HU_ASSERT_EQ(err, HU_ERR_NOT_SUPPORTED);

    hu_style_fingerprint_t fp;
    err = hu_style_fingerprint_get(&mem, &alloc, "x", 1, &fp);
    HU_ASSERT_EQ(err, HU_ERR_NOT_SUPPORTED);

    mem.vtable->deinit(mem.ctx);
}

void run_style_tracker_tests(void) {
    HU_TEST_SUITE("style_tracker");
    HU_RUN_TEST(style_update_haha_sets_laugh_style_and_lowercase);
    HU_RUN_TEST(style_update_periods_sets_uses_periods);
    HU_RUN_TEST(style_get_build_directive_contains_hints);
    HU_RUN_TEST(style_no_fingerprint_build_directive_returns_zero);
    HU_RUN_TEST(style_get_nonexistent_returns_zeroed);
    HU_RUN_TEST(style_none_memory_returns_not_supported);
}

#else

void run_style_tracker_tests(void) {
    (void)0;
}

#endif /* HU_ENABLE_SQLITE */
