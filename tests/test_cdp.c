#include "human/pwa/cdp.h"
#include "test_framework.h"
#include <string.h>

static void *cdp_alloc_fn(void *ctx, size_t size) { (void)ctx; return malloc(size); }
static void *cdp_realloc_fn(void *ctx, void *ptr, size_t old_size, size_t new_size) {
    (void)ctx; (void)old_size; return realloc(ptr, new_size);
}
static void cdp_free_fn(void *ctx, void *ptr, size_t size) { (void)ctx; (void)size; free(ptr); }

static hu_allocator_t cdp_alloc;
static void cdp_setup(void) {
    cdp_alloc = (hu_allocator_t){
        .alloc = cdp_alloc_fn, .realloc = cdp_realloc_fn, .free = cdp_free_fn, .ctx = NULL,
    };
}

static void cdp_connect_returns_mock(void) {
    cdp_setup();
    hu_cdp_session_t s;
    HU_ASSERT_EQ(hu_cdp_connect(&cdp_alloc, "localhost", 9222, &s), HU_OK);
    HU_ASSERT(s.connected);
    HU_ASSERT_NOT_NULL(s.ws_url);
    hu_cdp_disconnect(&s);
}

static void cdp_navigate_succeeds(void) {
    cdp_setup();
    hu_cdp_session_t s;
    hu_cdp_connect(&cdp_alloc, "localhost", 9222, &s);
    HU_ASSERT_EQ(hu_cdp_navigate(&s, "https://example.com", 19), HU_OK);
    hu_cdp_disconnect(&s);
}

static void cdp_evaluate_returns_result(void) {
    cdp_setup();
    hu_cdp_session_t s;
    hu_cdp_connect(&cdp_alloc, "localhost", 9222, &s);
    char *result = NULL;
    size_t result_len = 0;
    HU_ASSERT_EQ(hu_cdp_evaluate(&s, "document.title", 14, &result, &result_len), HU_OK);
    HU_ASSERT_NOT_NULL(result);
    HU_ASSERT(result_len > 0);
    cdp_alloc.free(cdp_alloc.ctx, result, result_len + 1);
    hu_cdp_disconnect(&s);
}

static void cdp_screenshot_returns_base64(void) {
    cdp_setup();
    hu_cdp_session_t s;
    hu_cdp_connect(&cdp_alloc, "localhost", 9222, &s);
    hu_cdp_screenshot_t shot;
    HU_ASSERT_EQ(hu_cdp_screenshot(&s, &shot), HU_OK);
    HU_ASSERT_NOT_NULL(shot.data_base64);
    cdp_alloc.free(cdp_alloc.ctx, shot.data_base64, shot.data_len + 1);
    hu_cdp_disconnect(&s);
}

static void cdp_click_and_type(void) {
    cdp_setup();
    hu_cdp_session_t s;
    hu_cdp_connect(&cdp_alloc, "localhost", 9222, &s);
    HU_ASSERT_EQ(hu_cdp_click(&s, 100, 200), HU_OK);
    HU_ASSERT_EQ(hu_cdp_type(&s, "hello", 5), HU_OK);
    hu_cdp_disconnect(&s);
}

static void cdp_get_title(void) {
    cdp_setup();
    hu_cdp_session_t s;
    hu_cdp_connect(&cdp_alloc, "localhost", 9222, &s);
    char *title = NULL;
    size_t title_len = 0;
    HU_ASSERT_EQ(hu_cdp_get_title(&s, &title, &title_len), HU_OK);
    HU_ASSERT_NOT_NULL(title);
    cdp_alloc.free(cdp_alloc.ctx, title, title_len + 1);
    hu_cdp_disconnect(&s);
}

static void cdp_query_elements(void) {
    cdp_setup();
    hu_cdp_session_t s;
    hu_cdp_connect(&cdp_alloc, "localhost", 9222, &s);
    hu_cdp_element_t elems[4];
    size_t count = 0;
    HU_ASSERT_EQ(hu_cdp_query_elements(&s, "button", 6, elems, 4, &count), HU_OK);
    HU_ASSERT_EQ(count, 1);
    HU_ASSERT_EQ(elems[0].x, 100);
    hu_cdp_disconnect(&s);
}

static void cdp_null_args_rejected(void) {
    HU_ASSERT_EQ(hu_cdp_connect(NULL, NULL, 0, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void cdp_disconnected_rejects(void) {
    cdp_setup();
    hu_cdp_session_t s;
    memset(&s, 0, sizeof(s));
    s.alloc = &cdp_alloc;
    HU_ASSERT_EQ(hu_cdp_navigate(&s, "url", 3), HU_ERR_INVALID_ARGUMENT);
}

void run_cdp_tests(void) {
    HU_TEST_SUITE("CDP");
    HU_RUN_TEST(cdp_connect_returns_mock);
    HU_RUN_TEST(cdp_navigate_succeeds);
    HU_RUN_TEST(cdp_evaluate_returns_result);
    HU_RUN_TEST(cdp_screenshot_returns_base64);
    HU_RUN_TEST(cdp_click_and_type);
    HU_RUN_TEST(cdp_get_title);
    HU_RUN_TEST(cdp_query_elements);
    HU_RUN_TEST(cdp_null_args_rejected);
    HU_RUN_TEST(cdp_disconnected_rejects);
}
