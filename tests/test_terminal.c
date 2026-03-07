/* Terminal capability and color output tests */
#include "seaclaw/terminal.h"
#include "test_framework.h"
#include <string.h>

static void test_color_level_enum_values(void) {
    SC_ASSERT_EQ(SC_COLOR_LEVEL_NONE, 0);
    SC_ASSERT_EQ(SC_COLOR_LEVEL_BASIC, 1);
    SC_ASSERT_EQ(SC_COLOR_LEVEL_256, 2);
    SC_ASSERT_EQ(SC_COLOR_LEVEL_TRUECOLOR, 3);
}

static void test_theme_enum_values(void) {
    SC_ASSERT_EQ(SC_THEME_DARK, 0);
    SC_ASSERT_EQ(SC_THEME_LIGHT, 1);
}

static void test_color_level_within_valid_range(void) {
    sc_color_level_t level = sc_terminal_color_level();
    SC_ASSERT_TRUE(level >= SC_COLOR_LEVEL_NONE && level <= SC_COLOR_LEVEL_TRUECOLOR);
}

static void test_color_level_caching(void) {
    sc_color_level_t a = sc_terminal_color_level();
    sc_color_level_t b = sc_terminal_color_level();
    SC_ASSERT_EQ(a, b);
}

static void test_theme_within_valid_range(void) {
    sc_theme_t theme = sc_terminal_theme();
    SC_ASSERT_TRUE(theme == SC_THEME_DARK || theme == SC_THEME_LIGHT);
}

static void test_theme_caching(void) {
    sc_theme_t a = sc_terminal_theme();
    sc_theme_t b = sc_terminal_theme();
    SC_ASSERT_EQ(a, b);
}

static void test_color_fg_returns_buf(void) {
    char buf[32];
    const char *ret = sc_color_fg(buf, 255, 128, 0);
    SC_ASSERT_NOT_NULL(ret);
    SC_ASSERT_TRUE(ret == buf);
}

static void test_color_bg_returns_buf(void) {
    char buf[32];
    const char *ret = sc_color_bg(buf, 0, 128, 255);
    SC_ASSERT_NOT_NULL(ret);
    SC_ASSERT_TRUE(ret == buf);
}

static void test_color_fg_output_format(void) {
    char buf[32];
    sc_color_fg(buf, 100, 100, 100);
    SC_ASSERT_TRUE(buf[0] == '\0' || (buf[0] == '\033' && buf[1] == '['));
}

static void test_color_bg_output_format(void) {
    char buf[32];
    sc_color_bg(buf, 100, 100, 100);
    SC_ASSERT_TRUE(buf[0] == '\0' || (buf[0] == '\033' && buf[1] == '['));
}

static void test_color_buf_null_terminated(void) {
    char buf[32];
    memset(buf, 0xff, sizeof(buf));
    sc_color_fg(buf, 0, 0, 0);
    SC_ASSERT_TRUE(strlen(buf) < sizeof(buf));
}

void run_terminal_tests(void) {
    SC_TEST_SUITE("terminal");
    SC_RUN_TEST(test_color_level_enum_values);
    SC_RUN_TEST(test_theme_enum_values);
    SC_RUN_TEST(test_color_level_within_valid_range);
    SC_RUN_TEST(test_color_level_caching);
    SC_RUN_TEST(test_theme_within_valid_range);
    SC_RUN_TEST(test_theme_caching);
    SC_RUN_TEST(test_color_fg_returns_buf);
    SC_RUN_TEST(test_color_bg_returns_buf);
    SC_RUN_TEST(test_color_fg_output_format);
    SC_RUN_TEST(test_color_bg_output_format);
    SC_RUN_TEST(test_color_buf_null_terminated);
}
