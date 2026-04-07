#include "test_framework.h"
#include "human/agent/humanness.h"
#include "human/agent.h"
#include "human/core/allocator.h"
#include <string.h>

/* ── Build turn context: basic functionality ─────────────────────────── */

static void build_context_null_agent_returns_error(void) {
    HU_ASSERT_EQ(hu_agent_build_turn_context(NULL), HU_ERR_INVALID_ARGUMENT);
}

static void build_context_skips_when_already_set(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_agent_t agent;
    memset(&agent, 0, sizeof(agent));
    agent.alloc = &alloc;
    agent.conversation_context = "existing context";
    agent.conversation_context_len = 16;

    hu_error_t err = hu_agent_build_turn_context(&agent);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(agent.conversation_context, "existing context");
    HU_ASSERT_EQ(agent.humanness_ctx_owned, false);
}

static void build_context_produces_empty_without_persona(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_agent_t agent;
    memset(&agent, 0, sizeof(agent));
    agent.alloc = &alloc;

    hu_error_t err = hu_agent_build_turn_context(&agent);
    HU_ASSERT_EQ(err, HU_OK);
    hu_agent_free_turn_context(&agent);
}

/* ── Free turn context ───────────────────────────────────────────────── */

static void free_context_null_safe(void) {
    hu_agent_free_turn_context(NULL);

    hu_agent_t agent;
    memset(&agent, 0, sizeof(agent));
    agent.humanness_ctx_owned = false;
    hu_agent_free_turn_context(&agent);
}

static void free_context_clears_owned(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *buf = (char *)alloc.alloc(alloc.ctx, 32);
    memcpy(buf, "test context", 12);
    buf[12] = '\0';

    hu_agent_t agent;
    memset(&agent, 0, sizeof(agent));
    agent.alloc = &alloc;
    agent.conversation_context = buf;
    agent.conversation_context_len = 12;
    agent.humanness_ctx_owned = true;

    hu_agent_free_turn_context(&agent);
    HU_ASSERT_NULL(agent.conversation_context);
    HU_ASSERT_EQ(agent.conversation_context_len, 0u);
    HU_ASSERT_EQ(agent.humanness_ctx_owned, false);
}

static void free_context_does_not_free_external(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_agent_t agent;
    memset(&agent, 0, sizeof(agent));
    agent.alloc = &alloc;
    agent.conversation_context = "daemon owned";
    agent.conversation_context_len = 12;
    agent.humanness_ctx_owned = false;

    hu_agent_free_turn_context(&agent);
    HU_ASSERT_STR_EQ(agent.conversation_context, "daemon owned");
    HU_ASSERT_EQ(agent.conversation_context_len, 12u);
}

/* ── Voice profile update ─────────────────────────────────────────────── */

static void voice_update_null_agent_returns_error(void) {
    HU_ASSERT_EQ(hu_agent_update_voice_profile(NULL, "hello", 5), HU_ERR_INVALID_ARGUMENT);
}

#ifdef HU_HAS_PERSONA
static void voice_update_uninitialized_returns_error(void) {
    hu_agent_t agent;
    memset(&agent, 0, sizeof(agent));
    agent.voice_profile_initialized = false;
    HU_ASSERT_EQ(hu_agent_update_voice_profile(&agent, "hello", 5), HU_ERR_INVALID_ARGUMENT);
}

static void voice_update_increments_interaction(void) {
    hu_agent_t agent;
    memset(&agent, 0, sizeof(agent));
    hu_voice_profile_init(&agent.voice_profile);
    agent.voice_profile_initialized = true;

    uint32_t before = agent.voice_profile.interaction_count;
    hu_agent_update_voice_profile(&agent, "hey how are you", 15);
    HU_ASSERT(agent.voice_profile.interaction_count > before);
}

static void voice_update_detects_emotion(void) {
    hu_agent_t agent;
    memset(&agent, 0, sizeof(agent));
    hu_voice_profile_init(&agent.voice_profile);
    agent.voice_profile_initialized = true;

    uint32_t before = agent.voice_profile.emotional_exchanges;
    hu_agent_update_voice_profile(&agent, "I feel so sad about this", 24);
    HU_ASSERT(agent.voice_profile.emotional_exchanges > before);
}

static void voice_update_detects_topic(void) {
    hu_agent_t agent;
    memset(&agent, 0, sizeof(agent));
    hu_voice_profile_init(&agent.voice_profile);
    agent.voice_profile_initialized = true;

    const char *long_msg =
        "I was thinking about the implications of quantum computing on cryptography and whether "
        "post-quantum encryption methods will be ready in time";
    uint32_t before = agent.voice_profile.shared_topics;
    hu_agent_update_voice_profile(&agent, long_msg, strlen(long_msg));
    HU_ASSERT(agent.voice_profile.shared_topics > before);
}

static void voice_profile_stages_evolve(void) {
    hu_agent_t agent;
    memset(&agent, 0, sizeof(agent));
    hu_voice_profile_init(&agent.voice_profile);
    agent.voice_profile_initialized = true;

    HU_ASSERT_EQ(agent.voice_profile.stage, HU_VOICE_FORMAL);
    for (int i = 0; i < 20; i++)
        hu_agent_update_voice_profile(&agent, "I feel happy about this topic discussion", 40);
    HU_ASSERT(agent.voice_profile.stage > HU_VOICE_FORMAL);
}
#endif /* HU_HAS_PERSONA */

/* ── Registration ─────────────────────────────────────────────────────── */

void run_humanness_context_tests(void) {
    HU_TEST_SUITE("Humanness Context");

    HU_RUN_TEST(build_context_null_agent_returns_error);
    HU_RUN_TEST(build_context_skips_when_already_set);
    HU_RUN_TEST(build_context_produces_empty_without_persona);
    HU_RUN_TEST(free_context_null_safe);
    HU_RUN_TEST(free_context_clears_owned);
    HU_RUN_TEST(free_context_does_not_free_external);
    HU_RUN_TEST(voice_update_null_agent_returns_error);
#ifdef HU_HAS_PERSONA
    HU_RUN_TEST(voice_update_uninitialized_returns_error);
    HU_RUN_TEST(voice_update_increments_interaction);
    HU_RUN_TEST(voice_update_detects_emotion);
    HU_RUN_TEST(voice_update_detects_topic);
    HU_RUN_TEST(voice_profile_stages_evolve);
#endif
}
