#include "human/channels/channel_embed.h"
#include "human/core/allocator.h"
#include "test_framework.h"
#include <string.h>

static void embed_format_discord_basic(void) {
    hu_allocator_t a = hu_system_allocator();
    hu_embed_t e = {0};
    e.type = HU_EMBED_RICH;
    e.title = "Test";
    e.description = "A test embed";
    e.color = 0x7AB648;
    char *out = NULL;
    size_t len = 0;
    HU_ASSERT_EQ(hu_embed_format_discord(&a, &e, &out, &len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT(len > 0);
    a.free(a.ctx, out, len + 1);
}

static void embed_format_null_args(void) {
    hu_allocator_t a = hu_system_allocator();
    hu_embed_t e = {0};
    char *out = NULL;
    size_t len = 0;
    HU_ASSERT_EQ(hu_embed_format_discord(NULL, &e, &out, &len), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_embed_format_discord(&a, NULL, &out, &len), HU_ERR_INVALID_ARGUMENT);
}

static void embed_deinit_handles_null(void) {
    hu_allocator_t a = hu_system_allocator();
    hu_embed_t e = {0};
    hu_embed_deinit(&e, &a);
    hu_embed_deinit(NULL, &a);
}

void run_channel_embeds_tests(void) {
    HU_TEST_SUITE("ChannelEmbeds");
    HU_RUN_TEST(embed_format_discord_basic);
    HU_RUN_TEST(embed_format_null_args);
    HU_RUN_TEST(embed_deinit_handles_null);
}
