#include "test_framework.h"
#include "human/security/normalize.h"

static void test_normalize_basic_lowercase(void) {
    char out[64];
    size_t len;
    HU_ASSERT_EQ(hu_normalize_confusables("HELLO", 5, out, sizeof(out), &len), HU_OK);
    HU_ASSERT_STR_EQ(out, "hello");
}

static void test_normalize_homoglyph_digits(void) {
    char out[64];
    size_t len;
    HU_ASSERT_EQ(hu_normalize_confusables("k1ll", 4, out, sizeof(out), &len), HU_OK);
    HU_ASSERT_STR_EQ(out, "kill");
}

static void test_normalize_at_dollar(void) {
    char out[64];
    size_t len;
    HU_ASSERT_EQ(hu_normalize_confusables("h@te$", 5, out, sizeof(out), &len), HU_OK);
    HU_ASSERT_STR_EQ(out, "hates");
}

static void test_normalize_collapse_spaces(void) {
    char out[64];
    size_t len;
    HU_ASSERT_EQ(hu_normalize_confusables("k i l l", 7, out, sizeof(out), &len), HU_OK);
    HU_ASSERT_STR_EQ(out, "kill");
    HU_ASSERT_EQ(len, (size_t)4);
}

static void test_normalize_strip_zwsp(void) {
    /* U+200B is E2 80 8B in UTF-8 */
    const char input[] = "k\xe2\x80\x8bill";
    char out[64];
    size_t len;
    HU_ASSERT_EQ(hu_normalize_confusables(input, sizeof(input) - 1, out, sizeof(out), &len), HU_OK);
    HU_ASSERT_STR_EQ(out, "kill");
}

static void test_normalize_strip_bom(void) {
    /* U+FEFF is EF BB BF in UTF-8 */
    const char input[] = "\xef\xbb\xbfhello";
    char out[64];
    size_t len;
    HU_ASSERT_EQ(hu_normalize_confusables(input, sizeof(input) - 1, out, sizeof(out), &len), HU_OK);
    HU_ASSERT_STR_EQ(out, "hello");
}

static void test_normalize_strips_nbsp(void) {
    char out[64];
    size_t len;
    /* "a\xC2\xA0b" = a + NBSP + b */
    HU_ASSERT_EQ(hu_normalize_confusables("a\xC2\xA0" "b", 4, out, sizeof(out), &len), HU_OK);
    HU_ASSERT_STR_EQ(out, "ab");
}

static void test_normalize_strips_em_space(void) {
    char out[64];
    size_t len;
    /* "a\xE2\x80\x83""b" = a + EM SPACE + b */
    HU_ASSERT_EQ(hu_normalize_confusables("a\xE2\x80\x83" "b", 5, out, sizeof(out), &len), HU_OK);
    HU_ASSERT_STR_EQ(out, "ab");
}

static void test_normalize_strips_ideographic_space(void) {
    char out[64];
    size_t len;
    /* "a\xE3\x80\x80""b" = a + Ideographic Space + b */
    HU_ASSERT_EQ(hu_normalize_confusables("a\xE3\x80\x80" "b", 5, out, sizeof(out), &len), HU_OK);
    HU_ASSERT_STR_EQ(out, "ab");
}

static void test_normalize_null_args(void) {
    char out[64];
    size_t len;
    HU_ASSERT_EQ(hu_normalize_confusables(NULL, 5, out, sizeof(out), &len), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_normalize_confusables("hi", 2, NULL, 64, &len), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_normalize_confusables("hi", 2, out, sizeof(out), NULL), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_normalize_confusables("hi", 2, out, 0, &len), HU_ERR_INVALID_ARGUMENT);
}

static void test_normalize_empty_input(void) {
    char out[64];
    size_t len;
    HU_ASSERT_EQ(hu_normalize_confusables("", 0, out, sizeof(out), &len), HU_OK);
    HU_ASSERT_EQ(len, (size_t)0);
    HU_ASSERT_EQ(out[0], '\0');
}

static void test_normalize_buffer_cap(void) {
    char out[4];
    size_t len;
    HU_ASSERT_EQ(hu_normalize_confusables("abcdefgh", 8, out, sizeof(out), &len), HU_OK);
    HU_ASSERT_EQ(len, (size_t)3);
    HU_ASSERT_STR_EQ(out, "abc");
}

void run_normalize_tests(void) {
    HU_TEST_SUITE("Confusable Normalization");
    HU_RUN_TEST(test_normalize_basic_lowercase);
    HU_RUN_TEST(test_normalize_homoglyph_digits);
    HU_RUN_TEST(test_normalize_at_dollar);
    HU_RUN_TEST(test_normalize_collapse_spaces);
    HU_RUN_TEST(test_normalize_strip_zwsp);
    HU_RUN_TEST(test_normalize_strip_bom);
    HU_RUN_TEST(test_normalize_strips_nbsp);
    HU_RUN_TEST(test_normalize_strips_em_space);
    HU_RUN_TEST(test_normalize_strips_ideographic_space);
    HU_RUN_TEST(test_normalize_null_args);
    HU_RUN_TEST(test_normalize_empty_input);
    HU_RUN_TEST(test_normalize_buffer_cap);
}
