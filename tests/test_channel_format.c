#include "human/channels/format.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "test_framework.h"
#include <string.h>

static void channel_format_imessage_strips_markdown_and_ai_phrases(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *in = "**Bold** As an AI I can help.";
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err =
        hu_channel_format_outbound(&alloc, "imessage", 8, in, strlen(in), &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT(strstr(out, "**") == NULL);
    HU_ASSERT(strstr(out, "As an AI") == NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void channel_format_slack_mrkdwn_link_and_bold(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *in = "See [doc](https://example.com/a) and **bold** here.";
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err =
        hu_channel_format_outbound(&alloc, "slack", 5, in, strlen(in), &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT(strstr(out, "<https://example.com/a|doc>") != NULL);
    HU_ASSERT(strstr(out, "*bold*") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void channel_format_discord_trims_trailing_ws(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *in = "keep\n  \t  ";
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err =
        hu_channel_format_outbound(&alloc, "discord", 7, in, strlen(in), &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_EQ(out_len, 4);
    HU_ASSERT(memcmp(out, "keep", 4) == 0);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void channel_format_email_wraps_paragraphs(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *in = "Hello **world**.\n\nSecond *line*.";
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err =
        hu_channel_format_outbound(&alloc, "email", 5, in, strlen(in), &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT(strstr(out, "<p>") != NULL);
    HU_ASSERT(strstr(out, "<strong>world</strong>") != NULL);
    HU_ASSERT(strstr(out, "<em>line</em>") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void channel_format_cli_passthrough(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *in = "plain **md**";
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err =
        hu_channel_format_outbound(&alloc, "cli", 3, in, strlen(in), &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_EQ(out_len, strlen(in));
    HU_ASSERT(memcmp(out, in, out_len) == 0);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void channel_strip_markdown_null_args(void) {
    char *out = NULL;
    size_t out_len = 0;
    hu_allocator_t alloc = hu_system_allocator();
    HU_ASSERT_EQ(hu_channel_strip_markdown(NULL, "x", 1, &out, &out_len), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_channel_strip_markdown(&alloc, "x", 1, NULL, &out_len), HU_ERR_INVALID_ARGUMENT);
}

void run_channel_format_tests(void) {
    HU_TEST_SUITE("channel_format");
    HU_RUN_TEST(channel_format_imessage_strips_markdown_and_ai_phrases);
    HU_RUN_TEST(channel_format_slack_mrkdwn_link_and_bold);
    HU_RUN_TEST(channel_format_discord_trims_trailing_ws);
    HU_RUN_TEST(channel_format_email_wraps_paragraphs);
    HU_RUN_TEST(channel_format_cli_passthrough);
    HU_RUN_TEST(channel_strip_markdown_null_args);
}
