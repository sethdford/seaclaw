#include "human/core/allocator.h"
#include "human/tts/cartesia_stream.h"
#include "test_framework.h"
#include <string.h>

static void cartesia_stream_mock_roundtrip(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_cartesia_stream_t *s = NULL;
    HU_ASSERT_EQ(hu_cartesia_stream_open(&alloc, "test-key", "voice-1", "sonic-test", &s), HU_OK);
    HU_ASSERT_NOT_NULL(s);

    HU_ASSERT_EQ(hu_cartesia_stream_send_generation(s, &alloc, "ctx-a", "Hello", true), HU_OK);

    void *pcm = NULL;
    size_t plen = 0;
    bool done = false;
    HU_ASSERT_EQ(hu_cartesia_stream_recv_next(s, &alloc, &pcm, &plen, &done), HU_OK);
    HU_ASSERT_NOT_NULL(pcm);
    HU_ASSERT_EQ(plen, 64u * sizeof(float));
    HU_ASSERT_FALSE(done);
    alloc.free(alloc.ctx, pcm, plen);

    HU_ASSERT_EQ(hu_cartesia_stream_recv_next(s, &alloc, &pcm, &plen, &done), HU_OK);
    HU_ASSERT_NULL(pcm);
    HU_ASSERT_TRUE(done);

    HU_ASSERT_EQ(hu_cartesia_stream_cancel_context(s, &alloc, "ctx-a"), HU_OK);
    hu_cartesia_stream_close(s, &alloc);
}

void run_cartesia_stream_tests(void) {
    HU_TEST_SUITE("Cartesia stream");
    HU_RUN_TEST(cartesia_stream_mock_roundtrip);
}
